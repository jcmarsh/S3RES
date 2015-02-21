#!/bin/bash

PLAYER_TIME=220s
BASIC_TIME=210s
ANOTHER_TIME=200s

# make sure bench_config.h has TIME_FULL_BENCH defined.

# Empty with no redundancy
cp /home/jcmarsh/research/PINT/controllers/configs/ping_pong_micro/NONE_empty.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > NONE_empty_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep $BASIC_TIME
	sleep 60
done

sleep 60

# Empty with SDR (just a watchdog basically)
cp /home/jcmarsh/research/PINT/controllers/configs/ping_pong_micro/SDR_empty.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > SDR_empty_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep $BASIC_TIME
	sleep 60
done

sleep 60

# Empty with DMR (so SDC detection)
cp /home/jcmarsh/research/PINT/controllers/configs/ping_pong_micro/DMR_empty.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > DMR_empty_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep $BASIC_TIME
	sleep 60
done

# Empty with full TMR
cp /home/jcmarsh/research/PINT/controllers/configs/ping_pong_micro/TMR_empty.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > TMR_empty_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep $BASIC_TIME
	sleep 60
done