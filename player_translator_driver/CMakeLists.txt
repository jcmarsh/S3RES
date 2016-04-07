CMAKE_MINIMUM_REQUIRED (VERSION 2.4 FATAL_ERROR)
PROJECT (translator_driver)

# Include this CMake module to get most of the settings needed to build
SET (CMAKE_MODULE_PATH "/usr/local/share/cmake/Modules")
INCLUDE (UsePlayerPlugin)

PLAYER_ADD_PLUGIN_DRIVER (translator_driver SOURCES translator_driver.cc ../src/replicas.c ../src/commtypes.c)

target_link_libraries(translator_driver TASLimited)
include_directories(translator_driver ../tas_lib/inc/)

INSTALL(FILES ./build/libtranslator_driver.so
  DESTINATION ./lib/)