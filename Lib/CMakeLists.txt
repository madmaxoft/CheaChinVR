
add_subdirectory(Asio/asio)
add_library(asio::standalone ALIAS standalone)

add_subdirectory(Json)

add_subdirectory(Fmt EXCLUDE_FROM_ALL)

add_subdirectory(NetSurveillancePp)

add_subdirectory(SQLiteCpp EXCLUDE_FROM_ALL)
