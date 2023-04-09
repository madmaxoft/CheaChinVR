cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(CheaChinVR LANGUAGES CXX)





# Enable multithreaded builds in MSVC:
if(MSVC)
      add_compile_options(/MP)
endif()





# Enable generating the tests from CMake:
list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
enable_testing()
if(MSVC)
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()





# Set the minimum Windows SDK target to Windows XP
if(WIN32)
	add_definitions(-D_WIN32_WINNT=0x0601)
endif()





# Pull in all the libraries used:
add_subdirectory(Lib)

# Add all the tests:
add_subdirectory(Apps)
