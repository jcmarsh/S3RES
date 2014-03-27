CMAKE_MINIMUM_REQUIRED (VERSION 2.4 FATAL_ERROR)
PROJECT (art_pot_driver)

# Include this CMake module to get most of the settings needed to build
SET (CMAKE_MODULE_PATH "/usr/local/share/cmake/Modules")
INCLUDE (UsePlayerPlugin)

PLAYER_ADD_PLUGIN_DRIVER (art_pot_driver SOURCES art_pot_driver.cc)

INSTALL(FILES ./build/libart_pot_driver.so
  DESTINATION ./lib/)