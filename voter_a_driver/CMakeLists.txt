CMAKE_MINIMUM_REQUIRED (VERSION 2.4 FATAL_ERROR)
PROJECT (voter_a_driver)

# Include this CMake module to get most of the settings needed to build
SET (CMAKE_MODULE_PATH "/usr/local/share/cmake/Modules")
INCLUDE (UsePlayerPlugin)

PLAYER_ADD_PLUGIN_DRIVER (voter_a_driver SOURCES voter_a_driver.cc)

INSTALL(FILES ./build/libvoter_a_driver.so
  DESTINATION ./lib/)