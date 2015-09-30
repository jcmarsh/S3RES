#!/bin/bash

PLAYER_TIME=920s
BASIC_TIME=910s
ANOTHER_TIME=900s

SIM_IP=192.168.100.1

PINT_DIR=/home/jcmarsh/research/PINT
CONFIG_DIR=$PINT_DIR/controllers/configs

# Check bench_config, do you want to record response times or restart times?

###### SMR CASE ##########################################################
cp $CONFIG_DIR/smr_empty.cfg ./config_plumber.cfg
# Empty Tri restart kill -9 Empty_01092
cp $CONFIG_DIR/empty_variants/Empty_01092 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > smr_empty_restart_01092_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > smr_empty_restart_injector_01092_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> smr_empty_restart_injector_01092_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> smr_empty_restart_injector_01092_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_02048
cp $CONFIG_DIR/empty_variants/Empty_02048 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > smr_empty_restart_02048_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > smr_empty_restart_injector_02048_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> smr_empty_restart_injector_02048_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_injector_02048_$index.txt
	sleep 60
done

###### DMR CASE ##########################################################
cp $CONFIG_DIR/dmr_empty.cfg ./config_plumber.cfg
# Empty DMR restart kill -9 Empty_01092
cp $CONFIG_DIR/empty_variants/Empty_01092 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > dmr_empty_restart_01092_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > dmr_empty_restart_injector_01092_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> dmr_empty_restart_injector_01092_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> dmr_empty_restart_injector_01092_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_02048
cp $CONFIG_DIR/empty_variants/Empty_02048 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > dmr_empty_restart_02048_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > dmr_empty_restart_injector_02048_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> dmr_empty_restart_injector_02048_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> dmr_empty_restart_injector_02048_$index.txt
	sleep 60
done

exit

###### TMR CASE ##########################################################
cp $CONFIG_DIR/tmr_empty.cfg ./config_plumber.cfg
# Empty Tri restart kill -9 Empty_01092
cp $CONFIG_DIR/empty_variants/Empty_01092 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_01092_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_01092_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_01092_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_01092_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_02048
cp $CONFIG_DIR/empty_variants/Empty_02048 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_02048_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_02048_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_02048_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_02048_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_03072
cp $CONFIG_DIR/empty_variants/Empty_03072 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_03072_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_03072_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_03072_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_03072_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_04096
cp $CONFIG_DIR/empty_variants/Empty_04096 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_04096_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_04096_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_04096_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_04096_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_06144
cp $CONFIG_DIR/empty_variants/Empty_06144 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_06144_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_06144_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_06144_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_06144_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_08192
cp $CONFIG_DIR/empty_variants/Empty_08192 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_08192_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_08192_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_08192_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_08192_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_12288
cp $CONFIG_DIR/empty_variants/Empty_12288 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_12288_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_12288_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_12288_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_12288_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_16384
cp $CONFIG_DIR/empty_variants/Empty_16384 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_16384_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_16384_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_16384_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_16384_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_24576
cp $CONFIG_DIR/empty_variants/Empty_24576 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_24576_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_24576_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_24576_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_24576_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_32768
cp $CONFIG_DIR/empty_variants/Empty_32768 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_32768_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_32768_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_32768_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_32768_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_49152
cp $CONFIG_DIR/empty_variants/Empty_49152 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_49152_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_49152_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_49152_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_49152_$index.txt
	sleep 60
done

# Empty Tri restart kill -9 Empty_65536
cp $CONFIG_DIR/empty_variants/Empty_65536 ./Empty
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > empty_restart_test_Empty_65536_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_Empty_65536_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> empty_restart_test_injector_Empty_65536_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_Empty_65536_$index.txt
	sleep 60
done

