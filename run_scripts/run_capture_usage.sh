output=Dropbox/research/EMSOFT16/mini_test/latest_run

sim_ip=192.168.0.101
pint_dir=/home/jcmarsh/research/PINT
config_dir=$pint_dir/controllers/configs

player_time=220s
basic_time=210s
another_time=200s

itars=2

# $1 # of iterations, $2 file prefix, $3 player time, $4 PINT time
runCapture () {
    echo "**** Starting runExperiment $2, $1 + 1 iterations ****"
    for index in `seq 0 $1`; do
	echo "**** Iteration $index ****"
	sudo timeout $3 player baseline.cfg > $2$index.txt &
	sudo python player_to_rt.py 16
	sleep 2
	timeout $4 $pint_dir/stage_control/basic $sim_ip &
	sleep 5
	ps -a > $2usage_$index.txt
	read -p "Press any key when the robot hits the green dot..." -n1 -s
	sudo python check_usage.py >> $2usage_$index.txt
	sleep 10
	sudo pkill timeout
	sleep 10
    done
    echo "**** Finished $2 ****"
    echo ""
}

# 00 NMR baseline
cp $config_dir/all_wl.cfg ./config_plumber.cfg
runCapture $itars nmr_laptop_ $player_time $basic_time
tar -cvf nmr_laptop.tar *.txt
cp nmr_laptop.tar ~/$output
rm *.txt
sleep 20

# 01 All SMR baseline
cp $config_dir/all_smr_wl.cfg ./config_plumber.cfg
runCapture $itars smr_laptop_ $player_time $basic_time
tar -cvf smr_laptop.tar *.txt
cp smr_laptop.tar ~/$output
rm *.txt
sleep 20

# 02 All DMR baseline
cp $config_dir/all_dmr_wl.cfg ./config_plumber.cfg
runCapture $itars dmr_laptop_ $player_time $basic_time
tar -cvf dmr_laptop.tar *.txt
cp dmr_laptop.tar ~/$output
rm *.txt
sleep 20

# 03 All TMR baseline
cp $config_dir/all_tmr_wl.cfg ./config_plumber.cfg
runCapture $itars tmr_laptop_ $player_time $basic_time
tar -cvf tmr_laptop.tar *.txt
cp tmr_laptop.tar ~/$output
rm *.txt
sleep 20
