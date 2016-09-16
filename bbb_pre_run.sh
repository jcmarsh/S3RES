#!/bin/bash

sudo insmod enable_arm_pmu.ko
sudo cpufreq-set -f 1000MHz

