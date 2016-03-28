#!/bin/bash

# Check Baseline performance (configurations with no faults injected).

pint_dir=/home/debian/research/PINT
output=Dropbox/Research/EMSOFT16/mini_test/latest_run

source ./common_test.sh

player_time=320s
basic_time=310s
another_time=300s

itars=9

# Now all with load

# 00 All baseline
cp $config_dir/all_wl.cfg ./config_plumber.cfg
runExperiment $itars baseline_all_wl_ $player_time $basic_time
tar -cvf baseline_all_wl.tar *.txt
sudo -u debian scp baseline_all_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
sleep 60

# 01 All SMR baseline
cp $config_dir/all_smr_wl.cfg ./config_plumber.cfg
runExperiment $itars baseline_all_smr_wl_ $player_time $basic_time
tar -cvf baseline_all_smr_wl.tar *.txt
sudo -u debian scp baseline_all_smr_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
sleep 60

# 02 All DMR baseline
cp $config_dir/all_dmr_wl.cfg ./config_plumber.cfg
runExperiment $itars baseline_all_dmr_wl_ $player_time $basic_time
tar -cvf baseline_all_dmr_wl.tar *.txt
sudo -u debian scp baseline_all_dmr_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
sleep 60

# 03 All TMR baseline
cp $config_dir/all_tmr_wl.cfg ./config_plumber.cfg
runExperiment $itars baseline_all_tmr_wl_ $player_time $basic_time
tar -cvf baseline_all_tmr_wl.tar *.txt
sudo -u debian scp baseline_all_tmr_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
sleep 60

# 04 Reactive TMR, other DMR baseline
cp $config_dir/rl_tmr_other_dmr_wl.cfg ./config_plumber.cfg
runExperiment $itars baseline_rl_tmr_other_dmr_wl_ $player_time $basic_time
tar -cvf baseline_rl_tmr_other_dmr_wl.tar *.txt
sudo -u debian scp baseline_rl_tmr_other_dmr_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
sleep 60

# 05 Artpot TMR, planning and mapper DMR baseline, Filter SMR
cp $config_dir/art_tmr_planning_dmr_filter_smr_wl.cfg ./config_plumber.cfg
runExperiment $itars art_tmr_planning_dmr_filter_smr_wl_ $player_time $basic_time
tar -cvf art_tmr_planning_dmr_filter_smr_wl.tar *.txt
sudo -u debian scp art_tmr_planning_dmr_filter_smr_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
sleep 60

# 06 Filter SMR, all others TMR
cp $config_dir/filter_smr_all_tmr_wl.cfg ./config_plumber.cfg
runExperiment $itars filter_smr_all_tmr_wl_ $player_time $basic_time
tar -cvf filter_smr_all_tmr_wl.tar *.txt
sudo -u debian scp filter_smr_all_tmr_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
sleep 60
