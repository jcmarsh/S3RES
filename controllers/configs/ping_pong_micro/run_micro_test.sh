#!/bin/bash

PLAYER_TIME=920s
BASIC_TIME=910s
ANOTHER_TIME=900s

SIM_IP=161.253.66.53

PINT_DIR=/home/jcmarsh/research/PINT
CONFIG_DIR=$PINT_DIR/controllers/configs

ITARS=1
# make sure bench_config.h has TIME_FULL_BENCH defined.

# Empty with no redundancy
cp $CONFIG_DIR/ping_pong_micro/NONE_empty.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > NONE_empty_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep $BASIC_TIME
	sleep 40
done

sleep 40

# Empty with SDR (just a watchdog basically)
cp $CONFIG_DIR/ping_pong_micro/SMR_empty.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > SMR_empty_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep $BASIC_TIME
	sleep 40
done

sleep 40

# Empty with DMR (so SDC detection)
cp $CONFIG_DIR/ping_pong_micro/DMR_empty.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > DMR_empty_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep $BASIC_TIME
	sleep 40
done

sleep 40

# Empty with full TMR
cp $CONFIG_DIR/ping_pong_micro/TMR_empty.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > TMR_empty_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep $BASIC_TIME
	sleep 60
done