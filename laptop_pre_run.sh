#!/bin/bash

echo 0 > /sys/devices/system/cpu/cpu4/online
echo 0 > /sys/devices/system/cpu/cpu5/online
echo 0 > /sys/devices/system/cpu/cpu6/online
echo 0 > /sys/devices/system/cpu/cpu7/online

cpufreq-set -c 0 -f 2300Mhz
cpufreq-set -c 1 -f 2300Mhz
cpufreq-set -c 2 -f 2300Mhz
cpufreq-set -c 3 -f 2300Mhz
