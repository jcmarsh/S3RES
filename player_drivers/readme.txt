These are stage drivers.

To compile, create a build directory. Run cmake there.
Then: 
  make
  sudo make install

art_pot_driver - Artificial Potential driver.
rm_test_driver - A thus far failed and abandoned attempt to create a new type of driver in Player: a redundantly threaded one. I theory this would automate the process of started the redundant processes of the specified type. Need to re-examine the threadedness of this. To specific to Player/Stage (removed 4/14/14).
voter_a_driver - First "working" attempt. This voter connects to three other threaded drivers (each art_pot_driver). The problem is that all of these drivers are threads, so there are no protection mechanisms between them (removed 4/13/14).
voter_b_driver - Working version. Provides the required interfaces to talk to three artificial potential controllers (not drivers).
