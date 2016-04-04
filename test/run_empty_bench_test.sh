#!/bin/bash

basic_time=900
sleep_time=910

runRTT() {
    echo "Running NMR case size $2" >> $1_comments.txt
    echo "Running NMR case size $2"
    date
    timeout $basic_time ./GEVoteTest -s $2 > $1_$2_NMR.txt &
    sleep 5
    ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> $1_comments.txt
    sleep $sleep_time

    echo "" >> $1_comments.txt
    echo "Running SMR case size $2" >> $1_comments.txt
    echo "Running SMR case size $2"
    date
    timeout $basic_time ./GEVoteTest -v VoterM -r SMR -s $2 > $1_$2_SMR.txt &
    sleep 5
    ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> $1_comments.txt
    sleep $sleep_time

    echo "" >> $1_comments.txt
    echo "Running DMR case size $2" >> $1_comments.txt
    echo "Running DMR case size $2"
    date
    timeout $basic_time ./GEVoteTest -v VoterM -r DMR -s $2 > $1_$2_DMR.txt &
    sleep 5
    ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> $1_comments.txt
    sleep $sleep_time

    echo "" >> $1_comments.txt
    echo "Running TMR case size $2" >> $1_comments.txt
    echo "Running TMR case size $2"
    date
    timeout $basic_time ./GEVoteTest -v VoterM -r TMR -s $2 > $1_$2_TMR.txt &
    sleep 5
    ps -Ao pid,cpuid,maj_flt,min_flt,rtprio,pri,nice,pcpu,stat,wchan:20,comm >> $1_comments.txt
    sleep $sleep_time

    echo "" >> $1_comments.txt
}

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

runRTT $1 8
runRTT $1 1024
runRTT $1 2048
runRTT $1 4096
