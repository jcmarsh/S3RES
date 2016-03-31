#!/bin/bash

pint_dir=/home/debian/research/PINT
# TODO: CHANGE THIS
#output=Dropbox/Research/EMSOFT16/mini_test/sim_two_latest

source ./common_test.sh

player_time=220s
basic_time=210s
another_time=200s

itars=9

# TODO: Add making sure all processes are cleanup up after each iterations (maybe add in common_test?)

# For BB (no TMR), just kill -9 for now.
# kill -9 and kill -s 38 (exec and cfe) for SMR, DMR, Art, and RL

# 00 SMR kill -9
cp $config_dir/all_smr_wl.cfg ./config_plumber.cfg
runExperimentFaults $itars kill_all_smr_wl_ 'kill -9' $player_time $basic_time $another_time
tar -cvf kill_all_smr_wl.tar *.txt
sudo -u debian scp kill_all_smr_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
sleep 60

# 01 All DMR baseline
cp $config_dir/all_dmr_wl.cfg ./config_plumber.cfg
runExperimentFaults $itars kill_all_dmr_wl_ 'kill -9' $player_time $basic_time $another_time
tar -cvf kill_all_dmr_wl.tar *.txt
sudo -u debian scp kill_all_dmr_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
sleep 60

# 02 Art kill -9 tests
cp $config_dir/art_tmr_planning_dmr_filter_smr_wl.cfg ./config_plumber.cfg
runExperimentFaults $itars kill_art_wl_ "kill -9" $player_time $basic_time $another_time
tar -cvf kill_art_wl.tar *.txt
sudo -u debian scp kill_art_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt
sleep 60

# 03 Reactive TMR, kill -9 tests
cp $config_dir/rl_tmr_other_dmr_wl.cfg ./config_plumber.cfg
runExperimentFaults $itars kill_rl_wl_ $player_time $basic_time $another_time
tar -cvf kill_rl_wl.tar *.txt
sudo -u debian scp kill_rl_wl.tar jcmarsh@$sim_ip:~/$output
rm *.txt

# 02 All TMR kill -s 37 tests (simulated sdc)
# 03 All TMR kill -s 38 tests (simulated control flow error)
