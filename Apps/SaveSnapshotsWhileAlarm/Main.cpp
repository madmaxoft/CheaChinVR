#include <iostream>
#include <filesystem>
#include <fstream>
#include "fmt/format.h"
#include "Recorder.hpp"
#include "Root.hpp"





/** Stores the info about a single device that should be monitored. */
struct MonitoredDevice
{
	std::string mHostName;
	uint16_t mPort;
	std::string mUserName;
	std::string mPassword;
	std::shared_ptr<NetSurveillancePp::Recorder> mRecorder;

	MonitoredDevice(std::string && aHostName, uint16_t aPort, std::string && aUserName, std::string && aPassword):
		mHostName(std::move(aHostName)),
		mPort(std::move(aPort)),
		mUserName(std::move(aUserName)),
		mPassword(std::move(aPassword))
	{
	}
};

using MonitoredDevicePtr = std::shared_ptr<MonitoredDevice>;

/** Stores one channel of a particular device for which an alarm has started. */
struct ActiveAlarm
{
	MonitoredDevicePtr mDevice;
	int mChannel;
};





/** The devices that should be monitored for alarms. */
std::vector<MonitoredDevicePtr> gMonitoredDevices;

/** All the alarms that are currently active (snapshots should be saved every second).
Protected against multithreaded access by gMtxActiveAlarms. */
std::vector<ActiveAlarm> gActiveAlarms;

/** The mutex protecting gActiveAlarms agains multithreaded access. */
std::mutex gMtxActiveAlarms;

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
	auto userName = aDeviceSpec.substr(0, idxColon);

	// Parse the password:
	auto idxAt = aDeviceSpec.find('@', idxColon + 1);
	if (idxAt == std::string::npos)
	{
		std::cerr << fmt::format("Invalid device spec '{}': No '@' after password found.\n", aDeviceSpec);
		return false;
	}
	auto password = aDeviceSpec.substr(idxColon + 1, idxAt - idxColon - 1);

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
		hostName = aDeviceSpec.substr(idxAt + 1, idxPortColon - idxAt - 1);
	}
	else
	{
		hostName = aDeviceSpec.substr(idxAt + 1);
	}

	gMonitoredDevices.push_back(std::make_shared<MonitoredDevice>(
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





/** Saves the actual received snapshot data for the device's channel. */
void saveSnapshotData(std::shared_ptr<MonitoredDevice> aDevice, int aChannel, const char * aData, size_t aSize)
{
	auto tt = std::time(nullptr);
	auto tm = std::localtime(&tt);
	auto yearStr = fmt::format("{:04d}", tm->tm_year + 1900);
	auto yearMonthStr = fmt::format("{}/{:02d}", yearStr, tm->tm_mon);
	auto dateStr = fmt::format("{}/{:02d}", yearMonthStr, tm->tm_mday);
	std::filesystem::create_directory(yearStr);
	std::filesystem::create_directory(yearMonthStr);
	std::filesystem::create_directory(dateStr);
	auto fnam = fmt::format(
		"{}/{:02d}_{:02d}_{:02d}_{}_{}_ch{}.jpg",
		dateStr, tm->tm_hour, tm->tm_min, tm->tm_sec, aDevice->mHostName, aDevice->mPort, aChannel
	);
	std::ofstream f;
	f.open(fnam.c_str(), std::ofstream::binary | std::ofstream::out);
	if (!f.is_open())
	{
		std::cerr << fmt::format(
			"Failed to write snapshot from device {}:{}, channel {}, to file {}.\n",
			aDevice->mHostName, aDevice->mPort, aChannel, fnam
		);
		return;
	}
	f.write(aData, aSize);
	f.close();
}





/** Saves a snapshot from the specified device's channel. */
void saveSnapshot(std::shared_ptr<MonitoredDevice> aDevice, int aChannel)
{
	aDevice->mRecorder->capturePicture(aChannel,
		[aDevice, aChannel](const std::error_code & aError, const char * aData, size_t aSize)
		{
			if (aError)
			{
				std::cerr << fmt::format("Failed to capture snapshot from device {}:{}: {}\n", aDevice->mHostName, aDevice->mPort, aError.message());
				return;
			}

			saveSnapshotData(aDevice, aChannel, aData, aSize);
		}
	);
}





/** Called whenever an alarm event comes from a device.
Schedules a snapshot of the affected channel to be saved every second until the alarm goes out. */
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
	if (aIsStart)
	{
		// Save the first snapshot of the series ASAP:
		saveSnapshot(aDevice, aChannel);

		// Add the device's channel to the active alarms:
		std::cout << fmt::format("AlarmStart: Device {}:{}, channel {}\n", aDevice->mHostName, aDevice->mPort, aChannel);
		std::unique_lock<std::mutex> lg(gMtxActiveAlarms);
		gActiveAlarms.push_back({aDevice, aChannel});
	}
	else
	{
		// Remove the device's channel from the active channels:
		std::cout << fmt::format("AlarmEnd: Device {}:{}, channel {}\n", aDevice->mHostName, aDevice->mPort, aChannel);
		std::unique_lock<std::mutex> lg(gMtxActiveAlarms);
		gActiveAlarms.erase(std::remove_if(gActiveAlarms.begin(), gActiveAlarms.end(),
			[&aDevice, aChannel](const auto & aAlarm)
			{
				return (aAlarm.mDevice == aDevice) && (aAlarm.mChannel == aChannel);
			}
		));
	}
}





/** Starts monitoring all devices in gMonitoredDevices.
Returns true on success, false on failure; prints failure reason to stderr. */
bool startMonitoring()
{
	bool hasFailed = false;
	size_t numLeft = gMonitoredDevices.size();  // number of devices left to report their connection state
	std::mutex mtxFinished;
	std::condition_variable cvFinished;
	std::cerr << fmt::format("Connecting to {} devices...\n", numLeft);
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
						std::cerr << fmt::format("Failed to connect to {}:{}: {}\n", dev->mHostName, dev->mPort, aError.message());
						hasFailed = true;
						cvFinished.notify_all();
						return;
					}
					std::cerr << fmt::format("Connected to {}:{}, starting alarm-monitoring.\n", dev->mHostName, dev->mPort);
				}  // lock
				using namespace std::placeholders;
				dev->mRecorder->monitorAlarms(std::bind(&onAlarm, std::ref(dev), _1, _2, _3, _4, _5));
				cvFinished.notify_all();
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

	std::cerr << "Connected to all and monitoring alarms.\n";
	return true;
}





/** Called periodically by ASIO.
Saves a snapshot for each active alarm's channel. */
void onTimer (asio::steady_timer & aTimer, const std::error_code & aError)
{
	if (aError)
	{
		std::cerr << fmt::format("Failed to set up timer: {}\n", aError.message());
		return;
	}

	// Get a copy of all the active alarms
	std::vector<ActiveAlarm> activeAlarms;
	{
		std::unique_lock<std::mutex> lock(gMtxActiveAlarms);
		activeAlarms = gActiveAlarms;
	}
	for (const auto & aa: activeAlarms)
	{
		saveSnapshot(aa.mDevice, aa.mChannel);
	}

	// Set up the next timer tick:
	aTimer.expires_after(asio::chrono::seconds(1));
	aTimer.async_wait(std::bind(&onTimer, std::ref(aTimer), std::placeholders::_1));
};





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

	// Set up the timer for periodic snapshotting:
	asio::steady_timer timer(NetSurveillancePp::Root::instance().ioContext(), asio::chrono::seconds(1));
	timer.async_wait(std::bind(&onTimer, std::ref(timer), std::placeholders::_1));

	// Run it all asynchronously:
	NetSurveillancePp::Root::instance().ioContext().run();
	return 0;
}
