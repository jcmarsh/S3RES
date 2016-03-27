#!/bin/bash

BASIC_TIME=900
SLEEP_TIME=910

if [ $# -lt 1 ]
then
    echo "Usage: sh ./run_empty_bench_test prepend_file"
    exit
fi

echo "Comments for Run:" > $1_comments.txt
cpufreq-info >> $1_comments.txt
ls -lh >> $1_comments.txt

cat ../include/bench_config.h >> $1_comments.txt
cat ../include/system_config.h >> $1_comments.txt

echo "Running NMR case" >> $1_comments.txt
echo "Running NMR case"
date
timeout $BASIC_TIME ./GenericEmptyVoteTest > $1_NMR.txt &
sleep 5
ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> $1_comments.txt
sleep $SLEEP_TIME

echo "" >> $1_comments.txt
echo "Running SMR case" >> $1_comments.txt
echo "Running SMR case"
date
timeout $BASIC_TIME ./GenericEmptyVoteTest VoterM SMR > $1_SMR.txt &
sleep 5
ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> $1_comments.txt
sleep $SLEEP_TIME

echo "" >> $1_comments.txt
echo "Running DMR case" >> $1_comments.txt
echo "Running DMR case"
date
timeout $BASIC_TIME ./GenericEmptyVoteTest VoterM DMR > $1_DMR.txt &
sleep 5
ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> $1_comments.txt
sleep $SLEEP_TIME

echo "" >> $1_comments.txt
echo "Running TMR case" >> $1_comments.txt
echo "Running TMR case"
date
timeout $BASIC_TIME ./GenericEmptyVoteTest VoterM TMR > $1_TMR.txt &
sleep 5
ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> $1_comments.txt
sleep $SLEEP_TIME

echo "" >> $1_comments.txt
