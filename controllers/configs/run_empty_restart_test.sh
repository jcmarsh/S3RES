#!/bin/bash

PLAYER_TIME=320s
BASIC_TIME=310s
ANOTHER_TIME=300s

PINT_DIR=/home/jcmarsh/research/PINT
CONFIG_DIR=$PINT_DIR/controllers/configs

# Empty Tri restart kill -9
cp $CONFIG_DIR/tri_empty.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic 127.0.0.1 &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_$index.txt
	sleep 60
done