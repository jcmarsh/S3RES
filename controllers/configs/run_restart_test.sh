#!/bin/bash

PLAYER_TIME=320s
BASIC_TIME=310s
ANOTHER_TIME=300s

if [ $# -lt 1 ]
then
    echo "Supply and argument for the command to pass to injector.py. Perhaps 'kill -9'"
    exit
fi

echo "Using command: " "${1}"

# All Tri restart -9 Filter tests
cp /home/jcmarsh/research/PINT/controllers/configs/all_tri.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > filter_restart_test_tri_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > filter_restart_test_tri_injector_$index.txt
	timeout $ANOTHER_TIME python injector.py "${1}" "Filter" >> filter_restart_test_tri_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> filter_restart_test_tri_injector_$index.txt
	sleep 60
done

# All Tri restart -9 ArtPot tests
cp /home/jcmarsh/research/PINT/controllers/configs/all_tri.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > artpot_restart_test_tri_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > artpot_restart_test_tri_injector_$index.txt
	timeout $ANOTHER_TIME python injector.py "${1}" "ArtPot" >> artpot_restart_test_tri_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> artpot_restart_test_tri_injector_$index.txt
	sleep 60
done

# All Tri restart -9 Mapper tests
cp /home/jcmarsh/research/PINT/controllers/configs/all_tri.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > mapper_restart_test_tri_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > mapper_restart_test_tri_injector_$index.txt
	timeout $ANOTHER_TIME python injector.py "${1}" "Mapper" >> mapper_restart_test_tri_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> mapper_restart_test_tri_injector_$index.txt
	sleep 60
done

# All Tri restart -9 AStar tests
cp /home/jcmarsh/research/PINT/controllers/configs/all_tri.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > astar_restart_test_tri_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > astar_restart_test_tri_injector_$index.txt
	timeout $ANOTHER_TIME python injector.py "${1}" "AStar" >> astar_restart_test_tri_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> astar_restart_test_tri_injector_$index.txt
	sleep 60
done