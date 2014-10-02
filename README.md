PINT
====

Problem Injection Named Thoughtfully

This is likely a temporary repo (thus the terrible name). Meant to simulate Single Event Upsets, and protect againts them using triple redundancy.

## Directories / Contents
* `./controllers/` - These controllers use file descriptors for comm, and can be restarted as needed.
  * `art_pot_p.c` - Uses the artificial potential method to generate control commands.
  * `benchmarker.c` - Sits between the translator and the controller. Repeats messages and times responses of the controller.
  * `empty.c` - The simplest controller possible, always sends the same command.
  * `voterb.c` - Manages three controllers (such as `art_pot_p.c`), restarting them if problems are detected.
* `./examples/` - Not sure. Need to investigate / clean up if this is no longer used.
* `./include/` -
* `./lib/` - PINT should be library. But it isn't yet.
* `./player_tanslator_driver/` - Interacts with Player / Stage as a plugin driver, and a controller (such as `benchmarker.c`).
  * `baseline.cfg` - Configuration file for Player.
  * `CMakeLists.txt` - Follows the Player plugin build system.
  * `translator_driver.cc` - The code.
* `./src/` - Source code for PINT, for now included in the controller compilation. Should be a library.
* `./tas_lib/` - From Taylor's work, a reduced and old version of TCS.
* `./tracee_progs/` - Not sure, seems related to the examples directory.

## Compiling and Running

Currently dependant on Player / Stage. 

The simulation computer runs a Player with a config such as ?, which sets up the 

