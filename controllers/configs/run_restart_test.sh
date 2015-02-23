#!/bin/bash

PLAYER_TIME=320s
BASIC_TIME=310s
ANOTHER_TIME=300s

# All Tri kill -9 Filter tests
cp /home/jcmarsh/research/PINT/controllers/configs/all_tri.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > filter_kill_test_artpot_tri_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > filter_kill_test_artpot_tri_injector_$index.txt
	timeout $ANOTHER_TIME python single_injector.py "kill -9" "Filter" >> filter_kill_test_artpot_tri_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> filter_kill_test_artpot_tri_injector_$index.txt
	sleep 60
done

# All Tri kill -9 ArtPot tests
cp /home/jcmarsh/research/PINT/controllers/configs/all_tri.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > artpot_kill_test_artpot_tri_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > artpot_kill_test_artpot_tri_injector_$index.txt
	timeout $ANOTHER_TIME python single_injector.py "kill -9" "ArtPot" >> artpot_kill_test_artpot_tri_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> artpot_kill_test_artpot_tri_injector_$index.txt
	sleep 60
done

# All Tri kill -9 Mapper tests
cp /home/jcmarsh/research/PINT/controllers/configs/all_tri.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > mapper_kill_test_artpot_tri_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > mapper_kill_test_artpot_tri_injector_$index.txt
	timeout $ANOTHER_TIME python single_injector.py "kill -9" "Mapper" >> mapper_kill_test_artpot_tri_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> mapper_kill_test_artpot_tri_injector_$index.txt
	sleep 60
done

# All Tri kill -9 AStar tests
cp /home/jcmarsh/research/PINT/controllers/configs/all_tri.cfg ./config_plumber.cfg
for index in `seq 0 1`; do
	timeout $PLAYER_TIME player baseline.cfg > astar_kill_test_artpot_tri_$index.txt &
	sleep 5
	timeout $BASIC_TIME ./controllers/c_cont/basic 127.0.0.1 &
	sleep 5
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > astar_kill_test_artpot_tri_injector_$index.txt
	timeout $ANOTHER_TIME python single_injector.py "kill -9" "AStar" >> astar_kill_test_artpot_tri_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -eo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> astar_kill_test_artpot_tri_injector_$index.txt
	sleep 60
done