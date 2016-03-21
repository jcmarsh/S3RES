#!/bin/bash

source common_test.sh

PLAYER_TIME=220s
BASIC_TIME=210s
ANOTHER_TIME=200s

OUTPUT=/home/jcmarsh/Dropbox/research/EMSOFT16/mini_test/latest_run

ITARS=9

# NEED TO UPDATE! AND TEST!
# TODO: Add making sure all processes are cleanup up after each iterations (maybe add in common_test?)

# 00 Art kill -9 tests
cp $CONFIG_DIR/art_tmr_planning_dmr_filter_smr_wl.cfg ./config_plumber.cfg
runExperimentFaults $ITARS art_kill_test_tri_wl_ "kill -9" $PLAYER_TIME $BASIC_TIME $ANOTHER_TIME
mv *.txt $OUTPUT/art_kill_wl/

sleep 60

# 01 All TMR kill -9 tests
cp $CONFIG_DIR/all_tmr_wl.cfg ./config_plumber.cf
runExperimentFaults $ITARS kill_test_tmr_wl_ "kill -9" $PLAYER_TIME $BASIC_TIME $ANOTHER_TIME
mv *.txt $OUTPUT/kill_wl/

sleep 60

# 02 All TMR kill -s 37 tests (simulated sdc)
cp $CONFIG_DIR/all_tmr_wl.cfg ./config_plumber.cfg
runExperimentFaults $ITARS sdc_test_tmr_wl_ "kill -s 37" $PLAYER_TIME $BASIC_TIME $ANOTHER_TIME
mv *.txt $OUTPUT/sdc_wl/

sleep 60

# 03 All TMR kill -s 38 tests (simulated control flow error)
cp $CONFIG_DIR/all_tmr_wl.cfg ./config_plumber.cfg
runExperimentFaults $ITARS freeze_test_tmr_wl_ "kill -s 38" $PLAYER_TIME $BASIC_TIME $ANOTHER_TIME
mv *.txt $OUTPUT/freeze_wl/

sleep 60

# 04 All TMR inject_error tests
cp $CONFIG_DIR/all_tri_wl.cfg ./config_plumber.cfg
runExperimentFaults $ITARS fault_test_tmr_wl_ "./inject_error" $PLAYER_TIME $BASIC_TIME $ANOTHER_TIME
mv *.txt $OUTPUT/fault_wl/
