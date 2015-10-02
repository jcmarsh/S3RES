#!/bin/bash

PLAYER_TIME=920s
BASIC_TIME=910s
ANOTHER_TIME=900s

SIM_IP=161.253.66.53

ITARS=1

PINT_DIR=/home/jcmarsh/research/PINT
CONFIG_DIR=$PINT_DIR/controllers/configs

# Check bench_config, do you want to record response times or restart times?

# Two args: $1 is the file name prefix, $2 is the postfix... kinda.
runExperiment () {
	cp $CONFIG_DIR/empty_variants/Empty_$2 ./Empty
	for index in `seq 0 $ITARS`; do
		timeout $PLAYER_TIME player baseline.cfg > $1_empty_restart_$2_$index.txt &
		sleep 5
		timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
		sleep 5
		ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > $1_empty_restart_injector_$2_$index.txt
		timeout $ANOTHER_TIME python injector.py "kill -9" "Empty" >> $1_empty_restart_injector_$2_$index.txt &
		sleep $ANOTHER_TIME
		ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> $1_empty_restart_injector_$2_$index.txt
		sleep 60
	done	
}

###### SMR CASE ##########################################################
cp $CONFIG_DIR/smr_empty.cfg ./config_plumber.cfg
runExperiment smr 01092
runExperiment smr 02048
runExperiment smr 03072
runExperiment smr 04096
runExperiment smr 06144
runExperiment smr 08192
runExperiment smr 12288
runExperiment smr 16384
runExperiment smr 24576
runExperiment smr 32768
runExperiment smr 49152
runExperiment smr 65536

###### DMR CASE ##########################################################
cp $CONFIG_DIR/dmr_empty.cfg ./config_plumber.cfg
runExperiment dmr 01092
runExperiment dmr 02048
runExperiment dmr 03072
runExperiment dmr 04096
runExperiment dmr 06144
runExperiment dmr 08192
runExperiment dmr 12288
runExperiment dmr 16384
runExperiment dmr 24576
runExperiment dmr 32768
runExperiment dmr 49152
runExperiment dmr 65536

###### TMR CASE ##########################################################
cp $CONFIG_DIR/tmr_empty.cfg ./config_plumber.cfg
runExperiment tmr 01092
runExperiment tmr 02048
runExperiment tmr 03072
runExperiment tmr 04096
runExperiment tmr 06144
runExperiment tmr 08192
runExperiment tmr 12288
runExperiment tmr 16384
runExperiment tmr 24576
runExperiment tmr 32768
runExperiment tmr 49152
runExperiment tmr 65536
