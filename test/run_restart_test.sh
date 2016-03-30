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
runRestart SMR GenericEmpty_00204K SMR

echo "Running DMR case" >> $1_comments.txt
echo "Running DMR case"
runRestart DMR GenericEmpty_00204K DMR

echo "Running TMR case" >> $1_comments.txt
echo "Running TMR case"
runRestart TMR GenericEmpty_00204K TMR

echo "With TMR and different sizes" >> $1_comments.txt
echo "With TMR and different sizes"
runRestart TMR GenericEmpty_00256K TMR_00256K
runRestart TMR GenericEmpty_00512K TMR_00512K
runRestart TMR GenericEmpty_01024K TMR_01024K
runRestart TMR GenericEmpty_02048K TMR_02048K
runRestart TMR GenericEmpty_04096K TMR_04096K
runRestart TMR GenericEmpty_08192K TMR_08192K
runRestart TMR GenericEmpty_16384K TMR_16384K
runRestart TMR GenericEmpty_32768K TMR_32768K
