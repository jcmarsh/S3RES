#!/bin/bash

# Check Baseline performance (configurations with no faults injected.

source common_test.sh

PLAYER_TIME=320s
BASIC_TIME=310s
ANOTHER_TIME=300s

OUTPUT=/home/jcmarsh/Dropbox/research/EMSOFT16/mini_test/latest_run

ITARS=9

# Now all with load

# 00 All baseline
cp $CONFIG_DIR/all_wl.cfg ./config_plumber.cfg
runExperiment $ITARS baseline_all_wl_ $PLAYER_TIME $BASIC_TIME
tar -cfv baseline_all_wl.tar *.txt
scp baseline_all_wl.tar test@$SIM_IP:~/

sleep 60

# 01 All SMR baseline
cp $CONFIG_DIR/all_smr_bb_wl.cfg ./config_plumber.cfg
runExperiment $ITARS baseline_all_smr_wl_ $PLAYER_TIME $BASIC_TIME
tar -cfv baseline_all_smr_wl.tar *.txt
scp baseline_all_smr_wl.tar test@$SIM_IP:~/

sleep 60

# 02 All DMR baseline (bb A* still SMR)
cp $CONFIG_DIR/all_dmr_bb_wl.cfg ./config_plumber.cfg
runExperiment $ITARS baseline_all_bb_dmr_wl_ $PLAYER_TIME $BASIC_TIME
tar -cfv baseline_all_bb_dmr_wl.tar *.txt
scp baseline_all_bb_dmr_wl.tar test@$SIM_IP:~/

sleep 60

# 03 All TMR baseline (bb A* still SMR)
cp $CONFIG_DIR/all_tmr_bb_wl.cfg ./config_plumber.cfg
runExperiment $ITARS baseline_all_bb_tmr_wl_ $PLAYER_TIME $BASIC_TIME
tar -cfv baseline_all_bb_tmr_wl.tar *.txt
scp baseline_all_bb_tmr_wl.tar test@$SIM_IP:~/
