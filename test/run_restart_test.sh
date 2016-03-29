#!/bin/bash

# Make sure that VoterM is built with #define TIME_RESTART_REPLICA
# Basic idea is that the generic_empty test starts everything,
# writes / reads data, while injector.py kills and VoterM times.

basic_time=900
another_time=890
sleep_time=910

prepend=$1
kill_cmd=$2

runRestart () {
	date
	timeout $basic_time ./GEVoteTest VoterM $1 $2 > ${prepend}_${3}.txt &
	sleep 4
	ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> ${prepend}_comments.txt
	sleep 2
	timeout $another_time python injector.py "False" "${kill_cmd}" "GenericEmpty" >> ${prepend}_${3}_injector.txt &
	sleep $sleep_time
}

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

cat ../include/bench_config.h >> $1_comments.txt
cat ../include/system_config.h >> $1_comments.txt

echo "Running SMR case" >> $1_comments.txt
echo "Running SMR case"
runRestart SMR GenericEmpty_0204K SMR

echo "Running DMR case" >> $1_comments.txt
echo "Running DMR case"
runRestart DMR GenericEmpty_0204K DMR

echo "Running TMR case" >> $1_comments.txt
echo "Running TMR case"
runRestart TMR GenericEmpty_0204K TMR

echo "With TMR and different sizes" >> $1_comments
echo "With TMR and different sizes"
runRestart TMR GenericEmpty_0256K TMR_0256K
runRestart TMR GenericEmpty_0512K TMR_0512K
runRestart TMR GenericEmpty_1024K TMR_1024K
runRestart TMR GenericEmpty_2048K TMR_2048K
runRestart TMR GenericEmpty_4096K TMR_4096K
runRestart TMR GenericEmpty_8192K TMR_8192K
