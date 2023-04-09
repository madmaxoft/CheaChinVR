set (SRC
	Main.cpp
)

set (HDR
)

add_executable(SaveSnapshotsWhileAlarm ${SRC} ${HDR})
target_link_libraries(SaveSnapshotsWhileAlarm PRIVATE NetSurveillancePp-static SQLiteCpp)
