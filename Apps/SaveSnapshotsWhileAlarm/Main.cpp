#include <iostream>
#include "fmt/format.h"
#include "Recorder.hpp"





struct MonitoredDevice
{
	std::string mHostName;
	uint16_t mPort;
	std::string mUserName;
	std::string mPassword;
	std::shared_ptr<NetSurveillancePp::Recorder> mRecorder;
	std::atomic<bool> mShouldScheduleSnapshots;

	MonitoredDevice(std::string && aHostName, uint16_t aPort, std::string && aUserName, std::string && aPassword):
		mHostName(std::move(aHostName)),
		mPort(std::move(aPort)),
		mUserName(std::move(aUserName)),
		mPassword(std::move(aPassword)),
		mShouldScheduleSnapshots(false)
	{
	}
};

using MonitoredDevicePtr = std::unique_ptr<MonitoredDevice>;





/** The devices that should be monitored for alarms. */
std::vector<MonitoredDevicePtr> gMonitoredDevices;

/** The name of the DB file where to store the snapshots.
If empty, the snapshots are stored in the current folder. */
std::string gDbFileName;





/** Parses the device specification and adds it to gMonitoredDevices.
Returns true on success, false on failure; prints failure reason to stderr.
The device spec is in the form "<username>:<password>@<hostname>[:<port>]" */
bool addMonitoredDevice(const std::string & aDeviceSpec)
{
	// Parse the username:
	auto idxColon = aDeviceSpec.find(':');
	if (idxColon == std::string::npos)
	{
		std::cerr << fmt::format("Invalid device spec '{}': No ':' after username found.\n", aDeviceSpec);
		return false;
	}
	auto userName = aDeviceSpec.substr(0, idxColon - 1);

	// Parse the password:
	auto idxAt = aDeviceSpec.find('@', idxColon + 1);
	if (idxAt == std::string::npos)
	{
		std::cerr << fmt::format("Invalid device spec '{}': No '@' after password found.\n", aDeviceSpec);
		return false;
	}
	auto password = aDeviceSpec.substr(idxColon + 1, idxAt - idxColon);

	uint16_t port = 34567;
	std::string hostName;
	auto idxPortColon = aDeviceSpec.find(':', idxAt + 1);
	if (idxPortColon != std::string::npos)
	{
		port = std::stoi(aDeviceSpec.substr(idxPortColon + 1));
		if (port == 0)
		{
			std::cerr << fmt::format("Invalid device spec '{}': Failed to parse port number in '{}'.\n", aDeviceSpec, aDeviceSpec.substr(idxPortColon + 1));
			return false;
		}
		hostName = aDeviceSpec.substr(idxAt + 1, idxPortColon - idxAt);
	}
	else
	{
		hostName = aDeviceSpec.substr(idxAt + 1);
	}

	gMonitoredDevices.push_back(std::make_unique<MonitoredDevice>(
		std::move(hostName), port, std::move(userName), std::move(password)
	));
	return true;
}





/** Parses the commandline params into the global variables.
Returns true on success, false on failure; prints failure reason to stderr.
The params are either device spec in the form "<username>:<password>@<hostname>[:<port>]",
or "--db" followed by the database filename. */
bool parseCommandLine(int aArgC, char ** aArgV)
{
	// Parse all cmdline params:
	for (int i = 1; i < aArgC; ++i)
	{
		// DB filename?
		if (strcmp(aArgV[i], "--db") == 0)
		{
			if (i == aArgC - 1)
			{
				std::cerr << "The --db parameter needs the database filename.\n";
				return false;
			}
			++i;
			gDbFileName = aArgV[i];
		}

		// Monitored device spec?
		if (!addMonitoredDevice(aArgV[i]))
		{
			return false;
		}
	}

	// Check that there's at least one vaild device:
	if (gMonitoredDevices.empty())
	{
		std::cerr << "No device to be monitored was specified.\n";
		std::cerr << "Use <username>:<password>@<hostname>[:<port>] parameter to specify a device.\n";
		return false;
	}
	return true;
}





/** Called whenever an alarm event comes from a device.
Saves a snapshot from the affected channel.
TODO: A snapshot should be saved every second until the alarm goes out. */
void onAlarm(
	MonitoredDevicePtr & aDevice,
	const std::error_code & aError,
	int aChannel,
	bool aIsStart,
	const std::string & aEventType,
	const nlohmann::json & aWholeJson
)
{
	if (aError)
	{
		std::cerr << fmt::format("Failed to monitor alarms on {}:{}: {}", aDevice->mHostName, aDevice->mPort, aError.message());
		return;
	}
	if (!aIsStart)
	{
		return;
	}

	// TODO: Save the snapshot
	// TODO: Schedule saving a snapshot every second until the alarm goes out
}





/** Starts monitoring all devices in gMonitoredDevices.
Returns true on success, false on failure; prints failure reason to stderr. */
bool startMonitoring()
{
	bool hasFailed = false;
	size_t numLeft = gMonitoredDevices.size();  // number of devices left to report their connection state
	std::mutex mtxFinished;
	std::condition_variable cvFinished;
	for (auto & dev: gMonitoredDevices)
	{
		dev->mRecorder = NetSurveillancePp::Recorder::create();
		dev->mRecorder->connectAndLogin(dev->mHostName, dev->mPort, dev->mUserName, dev->mPassword,
			[&hasFailed, &numLeft, &mtxFinished, &cvFinished, &dev](const std::error_code & aError)
			{
				{
					std::unique_lock<std::mutex> lock(mtxFinished);
					numLeft -= 1;
					if (aError)
					{
						std::cerr << fmt::format("Failed to connect to {}:{}: {}\n", dev->mHostName, dev->mPort, aError.message());\
						hasFailed = true;
						cvFinished.notify_all();
						return;
					}
				}  // lock
				using namespace std::placeholders;
				dev->mRecorder->monitorAlarms(std::bind(&onAlarm, std::ref(dev), _1, _2, _3, _4, _5));
			}
		);
	}

	// Wait for all devices to either conncet or fail connecting:
	std::unique_lock<std::mutex> lock(mtxFinished);
	while (numLeft > 0)
	{
		cvFinished.wait(lock);
	}
	if (hasFailed)
	{
		// The failure reason has already been printed in the callback.
		return false;
	}

	// TODO: Start monitoring alarms
	return true;
}





int main(int aArgC, char ** aArgV)
{
	if (!parseCommandLine(aArgC, aArgV))
	{
		std::cerr << "Failed to parse commandline.\n";
		return 1;
	}
	if (!startMonitoring())
	{
		std::cerr << "Failed to start monitoring.\n";
		return 2;
	}
	return 0;
}
