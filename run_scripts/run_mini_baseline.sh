#!/bin/bash

# Check Baseline performance (configurations with no faults injected.

source common_test.sh

PLAYER_TIME=220s
BASIC_TIME=210s
ANOTHER_TIME=200s

OUTPUT=/home/jcmarsh/Dropbox/research/EMSOFT16/mini_test/latest_run

ITARS=9

# Now all with load

# 00 All baseline
cp $CONFIG_DIR/all_wl.cfg ./config_plumber.cfg
runExperiment $ITARS baseline_all_wl_ $PLAYER_TIME $BASIC_TIME
mv *.txt $OUTPUT/all_wl/

sleep 60

# 01 All SMR baseline
cp $CONFIG_DIR/all_smr_wl.cfg ./config_plumber.cfg
runExperiment $ITARS baseline_all_smr_wl_ $PLAYER_TIME $BASIC_TIME
mv *.txt $OUTPUT/all_smr_wl/

sleep 60

# 02 All DMR baseline
cp $CONFIG_DIR/all_dmr_wl.cfg ./config_plumber.cfg
runExperiment $ITARS baseline_all_dmr_wl_ $PLAYER_TIME $BASIC_TIME
mv *.txt $OUTPUT/all_dmr_wl/

sleep 60

# 03 All Tri baseline
cp $CONFIG_DIR/all_tmr_wl.cfg ./config_plumber.cfg
runExperiment $ITARS baseline_all_tmr_wl_ $PLAYER_TIME $BASIC_TIME
mv *.txt $OUTPUT/all_tmr_wl/

sleep 60

# 04 Reactive TMR, other DMR baseline
cp $CONFIG_DIR/rl_tmr_other_dmr_wl.cfg ./config_plumber.cfg
runExperiment $ITARS baseline_rl_tmr_other_dmr_wl_ $PLAYER_TIME $BASIC_TIME
mv *.txt $OUTPUT/rl_other_wl/

sleep 60

# 05 Artpot TMR, planning and mapper DMR baseline, Filter SMR
cp $CONFIG_DIR/art_tmr_planning_dmr_filter_smr_wl.cfg ./config_plumber.cfg
runExperiment $ITARS baseline_art_tmr_planning_dmr_filter_smr_wl_ $PLAYER_TIME $BASIC_TIME
mv *.txt $OUTPUT/art_wl/

sleep 60

# 06 Filter SMR, all others TMR
cp $CONFIG_DIR/filter_smr_all_tmr_wl.cfg ./config_plumber.cfg
runExperiment $ITARS baseline_filter_smr_wl_ $PLAYER_TIME $BASIC_TIME
mv *.txt $OUTPUT/filter_wl/
