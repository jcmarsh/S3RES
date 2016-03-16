#!/bin/bash

PLAYER_TIME=220s
BASIC_TIME=210s
ANOTHER_TIME=200s

SIM_IP=192.168.0.101

PINT_DIR=/home/jcmarsh/research/PINT
CONFIG_DIR=$PINT_DIR/controllers/configs
OUTPUT=/home/jcmarsh/Dropbox/research/MyPaperAttempts/RTAS16/mini_test/latest_run

ITARS=99

# Now all with load

# All baseline
cp $CONFIG_DIR/all_wl.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > baseline_all_wl_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep $BASIC_TIME
	sleep 30
done

mv *.txt $OUTPUT/all_wl/

sleep 60

# All Tri baseline
cp $CONFIG_DIR/all_tri_wl.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > baseline_all_tri_wl_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep $BASIC_TIME
	sleep 30
done

mv *.txt $OUTPUT/all_tri_wl/

sleep 60

# Reactive Tri, other DMR baseline
cp $CONFIG_DIR/rl_tri_other_dmr_wl.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > baseline_rl_tri_other_dmr_wl_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep $BASIC_TIME
	sleep 30
done

mv *.txt $OUTPUT/rl_other_wl/

sleep 60

# Artpot Tri, planning and mapper DMR baseline, Filter SMR
cp $CONFIG_DIR/art_tmr_planning_dmr_filter_smr_wl.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > baseline_art_tmr_planning_dmr_filter_smr_wl_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep $BASIC_TIME
	sleep 30
done

mv *.txt $OUTPUT/art_wl/

sleep 60

# Artpot Tri, planning and mapper DMR baseline, Filter SMR
cp $CONFIG_DIR/filter_smr_all_tmr_wl.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > baseline_filter_smr_wl_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep $BASIC_TIME
	sleep 30
done

mv *.txt $OUTPUT/filter_wl/

sleep 60

# Art kill -9 tests
cp $CONFIG_DIR/art_tmr_planning_dmr_filter_smr_wl.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > art_kill_test_tri_wl_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > art_kill_test_tri_injector_wl_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" >> art_kill_test_tri_injector_wl_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> art_kill_test_tri_injector_wl_$index.txt
	sleep 40
done

mv *.txt $OUTPUT/art_kill_wl/

sleep 60

# All Tri kill -9 tests
cp $CONFIG_DIR/all_tri_wl.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > kill_test_tri_wl_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > kill_test_tri_injector_wl_$index.txt
	timeout $ANOTHER_TIME python injector.py "kill -9" >> kill_test_tri_injector_wl_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> kill_test_tri_injector_wl_$index.txt
	sleep 40
done

mv *.txt $OUTPUT/kill_wl/

sleep 60

# All Tri /bin/kill -s 37 tests (simulated sdc)
cp $CONFIG_DIR/all_tri_wl.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > sdc_test_tri_wl_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > sdc_test_tri_injector_wl_$index.txt
	timeout $ANOTHER_TIME python injector.py "/bin/kill -s 37" >> sdc_test_tri_injector_wl_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> sdc_test_tri_injector_wl_$index.txt
	sleep 40
done

mv *.txt $OUTPUT/sdc_wl/

sleep 60

# All Tri /bin/kill -s 38 tests (simulated control flow error)
cp $CONFIG_DIR/all_tri_wl.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > freeze_test_tri_wl_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > freeze_test_tri_injector_wl_$index.txt
	timeout $ANOTHER_TIME python injector.py "/bin/kill -s 38" >> freeze_test_tri_injector_wl_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> freeze_test_tri_injector_wl_$index.txt
	sleep 40
done

mv *.txt $OUTPUT/freeze_wl/

sleep 60

# All Tri inject_error tests
cp $CONFIG_DIR/all_tri_wl.cfg ./config_plumber.cfg
for index in `seq 0 $ITARS`; do
	timeout $PLAYER_TIME player baseline.cfg > fault_test_tri_wl_$index.txt &
	sleep 5
	timeout $BASIC_TIME $PINT_DIR/stage_control/basic $SIM_IP &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > fault_test_tri_injector_wl_$index.txt
	timeout $ANOTHER_TIME python injector.py "./inject_error" >> fault_test_tri_injector_wl_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> fault_test_tri_injector_wl_$index.txt
	sleep 40
done

mv *.txt $OUTPUT/fault_wl/
