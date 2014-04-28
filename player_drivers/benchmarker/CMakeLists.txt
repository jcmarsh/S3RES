CMAKE_MINIMUM_REQUIRED (VERSION 2.4 FATAL_ERROR)
PROJECT (benchmarker_driver)

# Include this CMake module to get most of the settings needed to build
SET (CMAKE_MODULE_PATH "/usr/local/share/cmake/Modules")
INCLUDE (UsePlayerPlugin)

PLAYER_ADD_PLUGIN_DRIVER (benchmarker_driver SOURCES benchmarker_driver.cc ../../src/cpu.cpp ../../src/time.cpp ../../src/scheduler.cpp ../../src/force.cpp ../../src/taslimited.cpp)

INSTALL(FILES ./build/libbenchmarker_driver.so
  DESTINATION ./lib/)