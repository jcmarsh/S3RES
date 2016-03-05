#!/bin/bash

# Make sure that VoterM is built with #define TIME_RESTART_REPLICA
# Basic idea is that the generic_empty test starts everything,
# writes / reads data, while injector.py kills and VoterM times.

BASIC_TIME=300
ANOTHER_TIME=290
SLEEP_TIME=310

if [ $# -lt 2 ]
then
    echo "First arg: prepend file name"
    echo "Second arg: argument for to pass to injector.py. Perhaps 'kill -9'"
    exit
fi

echo "Comments for Run:" > $1_comments.txt
echo "Using command: ${1}" >> $1_comments.txt
cpufreq-info >> $1_comments.txt
ls -lh >> $1_comments.txt

echo "Running SMR case" >> $1_comments.txt
echo "Running SMR case"
date
timeout $BASIC_TIME ./GEVoteTest VoterM SMR > $1_SMR.txt &
ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> $1_comments.txt
sleep 2
timeout $ANOTHER_TIME python injector.py "False" "${2}" "GenericEmpty" >> $1_SMR_injector.txt &
sleep $SLEEP_TIME

echo "Running DMR case" >> $1_comments.txt
echo "Running DMR case"
date
timeout $BASIC_TIME ./GEVoteTest VoterM SMR > $1_DMR.txt &
ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> $1_comments.txt
sleep 2
timeout $ANOTHER_TIME python injector.py "False" "${2}" "GenericEmpty" >> $1_DMR_injector.txt &
sleep $SLEEP_TIME


echo "Running TMR case" >> $1_comments.txt
echo "Running TMR case"
date
timeout $BASIC_TIME ./GEVoteTest VoterM TMR > $1_TMR.txt &
ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> $1_comments.txt
sleep 2
timeout $ANOTHER_TIME python injector.py "False" "${2}" "GenericEmpty" >> $1_TMR_injector.txt &
sleep $SLEEP_TIME
