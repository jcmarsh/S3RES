CMAKE_MINIMUM_REQUIRED (VERSION 2.4 FATAL_ERROR)
PROJECT (rm_test_driver)

# Include this CMake module to get most of the settings needed to build
SET (CMAKE_MODULE_PATH "/usr/local/share/cmake/Modules")
INCLUDE (UsePlayerPlugin)

include_directories(../include/)

PLAYER_ADD_PLUGIN_DRIVER (rm_test_driver SOURCES rm_test_driver.cc)

INSTALL(FILES ./build/librm_test_driver.so
  DESTINATION ./lib/)