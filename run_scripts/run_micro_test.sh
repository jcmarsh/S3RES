#!/bin/bash

source common_test.sh

PLAYER_TIME=920s
BASIC_TIME=910s

ITARS=1
# make sure bench_config.h has TIME_FULL_BENCH defined.

# Can use Generic_Empty and TEST_IPC_ROUND in bench_config.h to check performance vs. msg length

# Empty with no redundancy
cp $CONFIG_DIR/none_empty.cfg ./config_plumber.cfg
runExperiment $ITARS NONE_empty_ $PLAYER_TIME $BASIC_TIME
sleep 60

# Empty with SDR (just a watchdog basically)
cp $CONFIG_DIR/smr_empty.cfg ./config_plumber.cfg
runExperiment $ITARS SMR_empty_ $PLAYER_TIME $BASIC_TIME
sleep 60

# Empty with DMR (so SDC detection)
cp $CONFIG_DIR/dmr_empty.cfg ./config_plumber.cfg
runExperiment $ITARS DMR_empty_ $PLAYER_TIME $BASIC_TIME
sleep 60

# Empty with full TMR
cp $CONFIG_DIR/tmr_empty.cfg ./config_plumber.cfg
runExperiment $ITARS TMR_empty_ $PLAYER_TIME $BASIC_TIME
sleep 60
