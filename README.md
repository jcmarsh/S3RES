PINT
====

Problem Injection Named Thoughtfully

This is likely a temporary repo (thus the terrible name). Meant to simulate Single Event Upsets, and protect against them using triple redundancy.

## Dependencies
* Player / Stage
* dietlibc (Just for Voters, makes them 40% smaller than musl)
* musl (Needed for Voters, recommended for components)
* libccv (for Load component).

Everything can be compiled just fine with glibc, but you will need to edit the Makefiles.

### musl

Project URL: http://www.musl-libc.org/

    wget http://www.musl-libc.org/releases/musl-1.1.14.tar.gz
    tar -xvf musl-1.1.14.tar.gz 
    cd musl-1.1.14
    ./configure 
    make
    sudo make install

You can test that this worked by going into the `tas_lib` directory and running `make`.

### Dietlibc

Project URL: https://www.fefe.de/dietlibc/

Version 0.33 does not support ARM7, so you must download from their repo (which is the recommended method anyways).

    cvs -d :pserver:cvs@cvs.fefe.de:/cvs -z9 co dietlibc
    cd dietlibc
    make

Unfortunately I got a bunch of errors here on the BeagleBone Black, dealing with libcrypt. So I edited the Makefile and removed all references to libcrypt, which did the trick.

Once built, the libraries will be in a directory `bin-ARCH/`, which can be installed like this:

    sudo install bin-arm/diet /usr/local/bin

## Compiling

PINT currently uses Player / Stage as a simulation environment. One machine will act as the simulator and needs to have Player and Stage installed, while the machines that are acting as the robots only require Player. One machine can be used for everything, which is convenient for testing an development. For experiments dealing with timing, using dedicate machines for each robot is required.

![Alternate setup options, one machine vs multiple](docs/alternate_setups.png?raw=true "Alternate Setups")

### Player

Currently using Player 3.1.0-SVN. To setup / install (from [Player FAQ](http://playerstage.sourceforge.net/wiki/Basic_FAQ)):

    svn checkout svn://svn.code.sf.net/p/playerstage/svn/code/player/trunk player
    cd player
    mkdir build
    ccmake ../

Most defaults at this step are fine, but you likely want deselect many of the drivers (greatly speeds up compilation). My setup has the following drivers: `differential fakelocalize gridmap lasertoranger mapcspace mapfile mapscale rangertolaser velcmd vfh vmapfile wavefront.` Boost is not required.

    make
    sudo make install
    player --version
    
The last command should show version 3.1.0-svn and all included drivers. You may need to ensure that player is in your library paths by adding one of the following to your `.bashrc` file (first works for ARM, second works for x86_64):

    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib:/usr/local/lib64

### Stage

TODO: Add instructions for install Stage and testing that it works with Player.

### PINT

Clone this repo. Will need to set the CPU speed. See bench_config.h for options. If real-time is of concern, make sure host machine is not running CPU scalling and that hyper threading is off. Likely want to skip libccv for now.

You will need to update the the file `directory_paths` so that the paths to PINT and Player source are correct. `TOP_LVL` should be set to directory which contains the PINT directory, `PLAYERC` to the actual player directory.

`tas_lib` should be made first:
  
    cd tas_lib
    make
    sudo make install

`player_translator_driver` is next. On the first make additional steps are needed:

    cd player_translator_driver
    mkdir build
    cd build
    ccmake ..
    make
    sudo make install

From the top level PINT directory, `make` and `make install`. Note that `make install` does not require `sudo`, because all it is doing is copying the executables to `../stage/experiments/`.
  

#### libccv needed for Load component.

Only used for the Load component; can skip this step so long as the appropriate Makefile is altered to not build Load. Install in directory containing PINT folder (I know).

* Download tar from http://libccv.org/
* Unzip: `tar -xf liuliu-ccv-785aba2.tar.gz` (filename may change)
* change directory name: `mv liuliu-ccv-785aba2 ccv`
* `cd ccv/lib/`
* `./configure` - check to see if anything you want is missing (libpng and libjpeg in the `LINK FLAGS` bit at the end)
* `make`
* copy `./lib/libccv.a` into a directory checked by ld (`/usr/local/lib/`)

On the BeagleBone Black `libccv` ran out of memory while building. I was able to remedy this by creating a large swap file on a microSD card and waiting a long time ([source](https://sheldondwill.wordpress.com/2013/12/14/beaglebone-black-ubuntu-adding-a-swapfile/)):

    (On the BeagleBone Black)
    cd /media/<Partition Label>
    sudo mkdir swap
    sudo dd if=/dev/zero of=./swap/swapfile bs=1M count=940
    sudo chmod 0600 ./swap/swapfile 
    sudo mkswap ./swap/swapfile
    sudo swapon ./swap/swapfile
    free -m 
    
If it worked, last like should show available swap: `Swap:          939          0        939`. I still had issues, and ended up commenting out the vgg sample.

## Running

To start the Stage simulator with a single robot on the simulation computer, from `stage/experiments` run:

     player simulator_only.cfg

To run the robot control program on the same machine you would use `single_computer.cfg` instead. The configuration files have some oddities that are artifacts from when they altered for each experimental run, which is no longer the case.

On the computer that will control the robot, first make sure that appropriate `pre_run.sh` script is run one time before starting any tests. It will do things like set cpu scaling, load the pmu module on the BBB, and disable hyper-threading on the quad-core system. Then you will start player with the appropriate config (needs to have the ip address of the simulation computer) to start the benchmarking plugin, set player to have an RT priority, kick off the simulation by connecting a client to player (normally it would control the robot, but instead the benchmarker launches `plumber` which sets everything up depending on `config_plumber.cfg`), and then launch whatever fault injection program you would like. Simple. It looks something like this:

    sudo player baseline.cfg &> ../nmr_usage_runs.txt &
    sudo python player_to_rt.py 16
    ./basic 192.168.0.104 &
    sudo python check_usage.py > nmr_bb_usage_02.txt

## Directory / Contents
* `./controllers/` - Components in a robotic control system. Each runs as a process, and communication is done through pipes
  * `art_pot.c` - Uses the artificial potential method to generate control commands for local navigation.
  * `a_star.c` - An A* implementation for path planning.
  * `benchmarker.c` - The entry and exit point for the system; sits between the player translator driver and the control system. Also launches Plumber to set up the rest of the system.
  * `empty.c` - Simplest component: sends back the same move command with every input.
  * `filter.c` - Perhaps should rename splitter: just repeats position and range data to multiple other components. Originally used a sliding window to average range readings, but this wasn't found to be very useful. May be useful if noise is introduced.
  * `logger.c` - Not intended for replication: used for measuring robot's task performance. Records the location readings of the robot in a text file (named based on date/time).
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
* `./player_translator_driver/` - Connects to the player instance running the simulator on a different machine. It translate between player and pipes. Launches and then connects to Benchmarker.
  * `baseline.cfg` - example player config
  * `translator_driver.cc` - The Player plugin driver.
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
