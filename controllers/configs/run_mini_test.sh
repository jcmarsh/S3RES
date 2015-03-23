#!/bin/bash

PLAYER_TIME=200s
BASIC_TIME=190s
ANOTHER_TIME=180s

PINT_DIR=/home/jcmarsh/research/PINT
CONFIG_DIR=$PINT_DIR/controllers/configs
OUTPUT=/home/jcmarsh/Dropbox/research/MyPaperAttempts/EMSOFT2015/mini_test_data/latest_run

ITARS=9

# All baseline
cp $CONFIG_DIR/all.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > baseline_all_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic 127.0.0.1 &
	sleep $BASIC_TIME
	sleep 30
done

mv *.txt $OUTPUT/all/

sleep 10

# All Tri baseline
cp $CONFIG_DIR/all_tri.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > baseline_all_tri_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic 127.0.0.1 &
	sleep $BASIC_TIME
	sleep 30
done

mv *.txt $OUTPUT/all_tri/

sleep 10

# Reactive Tri, other DMR baseline
cp $CONFIG_DIR/rl_tri_other_dmr.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > baseline_rl_tri_other_dmr_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic 127.0.0.1 &
	sleep $BASIC_TIME
	sleep 30
done

mv *.txt $OUTPUT/rl_other/

sleep 10

# Artpot Tri, planning and mapper DMR baseline, Filter SMR
cp $CONFIG_DIR/art_tmr_planning_dmr_filter_smr.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > baseline_art_tmr_planning_dmr_filter_smr_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic 127.0.0.1 &
	sleep $BASIC_TIME
	sleep 30
done

mv *.txt $OUTPUT/art/

sleep 10

# All Tri kill -9 tests
cp $CONFIG_DIR/all_tri.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > kill_test_tri_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic 127.0.0.1 &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > kill_test_tri_injector_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" >> kill_test_tri_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> kill_test_tri_injector_$index.txt
	sleep 40
done

mv *.txt $OUTPUT/kill/

sleep 10

# All Tri /bin/kill -s SIGRTMIN+2 tests (SDC can't be reliably detected on AStar, so neglect here)
cp $CONFIG_DIR/all_tri.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > sdc_test_tri_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic 127.0.0.1 &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > sdc_test_tri_injector_$index.txt
	#timeout $ANOTHER_TIME python injector.py "/bin/kill -s SIGRTMIN+2" "ArtPot" "Filter" "Mapper" >> sdc_test_tri_injector_$index.txt &
	timeout $ANOTHER_TIME python injector.py "/bin/kill -s SIGRTMIN+2" >> sdc_test_tri_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> sdc_test_tri_injector_$index.txt
	sleep 40
done

mv *.txt $OUTPUT/sdc/