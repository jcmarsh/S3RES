PINT
====

Problem Injection Named Thoughtfully

This is likely a temporary repo (thus the terrible name). Meant to simulate Single Event Upsets, and protect againts them using triple redundancy.

## Directorie / Contents
* `./controllers/` - Components in a robotic control system. Each runs as a process, and communication is done through pipes
  * `art_pot.c` - Uses the artificial potential method to generate control commands for local navigation.
  * `a_star.c` - An A* implementation for path planning.
  * `benchmarker.c` - The entry and exit point for the system; sits between the player translator driver and the control system. Also launches Plumber to set up the rest of the system.
  * `empty.c` - Simpliest component: sends back the same move command with every input.
  * `filter.c` - Perhaps should rename spliter: just repeats position and range data to multiple other components. Originally used a sliding window to average range readings, but this wasn't found to be very useful. May be useful if noise is introduced.
  * `logger.c` - Not intened for replication: used for measuring robot's task performance. Records the location readings of the robot in a text file (named based on date/time).
  * `mapper.c` - Creates a 2d map of the environment from range readings. Has a configurable threshold (x readings before considered an obstacle) and granularity setting (shared with AStar... check mapping.h).
  * `./configs/` - Descriptions of possible control systems
    * `all.cfg` - Every component, with no redundancy
    * `all_dmr.cfg` - Every component, with DMR
    * `all_tri.cfg` - Every component, with TMR
    * `art_tmr_planning_dmr_filter_smr.cfg` -
    * `filter_smr_all_tmr.cfg` -
    * `rl_tri.cfg` -
    * `rl_tri_other_dmr.cfg` -
    * `run_empty_restart_test.sh` -
    * `run_mini_test.sh` -
    * `tri_empty.cfg` -
    * `./ping_pong_micro/` - Used for round-trip benchmarking with empty controllers
* `./fault_injection/` - Utilities for injecting faults into components.
  * `injector.py` - Python script for selecting a process to inject an error into via supplied list of names, and then executing the specified command on that process' pid (such as 'kill -9')
  * `print_registers.h` - Useful macro for printing a `user_regs_struct`
  * `register_util.c` - Used to inject a random bit-flip into the given pid using ptrace to snoop registers.
* `./voter/` -
  * `voterd.c` -
* `./include/` - See comments for `/src/`
  * `bench_config.h` - Defines that select what timing is done
  * `commtypes.h` - Data types based between components
  * `controller.h` -
  * `cpu.h` - from TAS
  * `fd_client.h` -
  * `fd_server.h` -
  * `force.h` -
  * `mapping.h` -
  * `replicas.h` -
  * `taslimited.h` -
  * `time.h` -
* `./player_translator_driver/` - Connects to the player instance running the simulator on a different machine. It translate between player and pipes. Launches and then connects to Benchmaker.
  * `baseline.cfg` - example player config
  * `translator_driver.cc` - The Player plugin-able driver.
* `./plumber/` - Need a way to describe a graph of components, and then initiate the system. Plumber does that.
  * `pb.l` and `pb.y` - The Flex and Bison files describing the custom language used. See `/controllers/configs/` for examples.
  * `plumbing.c` -
  * `plumbing.h` -
* `./src/` - This directory needs to be reconsidered. `commtypes.c` and `mapping.c` should only be used by controllers, the rest only by PINT.
  * `commtypes.c` -
  * `controller.c` -
  * `fd_client.c` -
  * `fd_server.c` -
  * `mapping.c` -
  * `replicas.c` -
* `./stage_control/` - Simple program to kick off the simulation.
* `./tas_lib/` - Library originally from James Taylor, heavily stripped down and converted to C.
* `./test/` - Test code to allow components to be run in isolation. Handy for debugging and finding memory leaks.
  * `./micro_test/` - Tests used for micro benchmarks.
* `./voter/` - Code for the Voter.

## Compiling and Running

Currently dependant on Player / Stage. 
