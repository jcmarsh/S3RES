#!/bin/bash

PLAYER_TIME=2720s
BASIC_TIME=2710s
ANOTHER_TIME=2700s

if [ $# -lt 1 ]
then
    echo "Supply and argument for the command to pass to injector.py. Perhaps 'kill -9'"
    exit
fi

echo "Using command: " "${1}"

# All Tri restart -9 Filter tests
for index in `seq 0 0`; do
	timeout $PLAYER_TIME ./EmptyTest &
	sleep 5
	timeout $BASIC_TIME ./VoterD Empty TMR 120000 60  > empty_restart_test_$index.txt &
	sleep 5
	ps -ao pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm > empty_restart_test_injector_$index.txt
	timeout $ANOTHER_TIME python injector.py "${1}" "Empty" >> empty_restart_test_injector_$index.txt &
	sleep $ANOTHER_TIME
	ps -ao pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm >> empty_restart_test_injector_$index.txt
	sleep 30
done
