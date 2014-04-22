These are stage drivers.

To compile, create a build directory. Run cmake there.
Then: 
  make
  sudo make install

art_pot_driver - Artificial Potential driver.

voter_b_driver - Working version. Provides the required interfaces to talk to three artificial potential controllers (not drivers).
VoterB needs a few bits of TAS; in a separate repo for now. See below.
../include/cpu.h
../include/time.h 
voter_b_driver/cpu.cpp
voter_b_driver/time.cpp
