#!/bin/bash

# Check Baseline performance (configurations with no faults injected).

pint_dir=/home/debian/research/PINT
output=Dropbox/Research/EMSOFT16/mini_test/bbb_latest_run

source ./common_test.sh

player_time=320s
basic_time=310s
another_time=300s

itars=9

# Now all with load

# 00 All baseline
cp $config_dir/all_wl.cfg ./config_plumber.cfg
runExperiment $itars baseline_all_wl_ $player_time $basic_time
tar -cvf baseline_all_bb_wl.tar *.txt
sudo -u debian scp baseline_all_bb_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
sleep 60

# 01 All SMR baseline
cp $config_dir/all_smr_bb_wl.cfg ./config_plumber.cfg
runExperiment $itars baseline_all_smr_wl_ $player_time $basic_time
tar -cvf baseline_all_bb_smr_wl.tar *.txt
sudo -u debian scp baseline_all_bb_smr_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt

sleep 60

# 02 All DMR baseline (bb A* still SMR)
cp $config_dir/all_dmr_bb_wl.cfg ./config_plumber.cfg
runExperiment $itars baseline_all_bb_dmr_wl_ $player_time $basic_time
tar -cvf baseline_all_bb_dmr_wl.tar *.txt
sudo -u debian scp baseline_all_bb_dmr_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt

sleep 60

# 03 All TMR baseline (bb A* still SMR)
cp $config_dir/all_tmr_bb_wl.cfg ./config_plumber.cfg
runExperiment $itars baseline_all_bb_tmr_wl_ $player_time $basic_time
tar -cvf baseline_all_bb_tmr_wl.tar *.txt
sudo -u debian scp baseline_all_bb_tmr_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
