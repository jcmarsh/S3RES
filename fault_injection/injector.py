# Should look for (via ps) process that need to die!
# Should find all eligible pids, then send a kill -9 (or -s USR2 (or use the handy c program here)).
# James Marshall

import random
import sys
import time
from subprocess import Popen, PIPE, call

def getPIDs(pids, names, cmd):
	proc = Popen(cmd, shell=True, stdout=PIPE)
	for line in proc.stdout:
		words = line.split()
		if len(words) < 5: # ignores <defunct> processes
			try:
				pids.append(int(words[0]))
				names.append(words[3])
			except ValueError:
				pass

if len(sys.argv) < 2:
	print "please supply command to execute on unspecting processes (for example, 'kill -9' or 'kill -s USR2'"
        sys.exit()

# This should be checked before running, but hey we are kill processes here
cmd_start = sys.argv[1]

while True:
	time.sleep(1/5.0)
	victim_pids = []
	victim_names = []
	victim_count = 2

	#getPIDs(victim_pids, victim_names, 'ps -a | grep "AStar" | grep -v "Test"')
	#getPIDs(victim_pids, victim_names, 'ps -a | grep "Filter" | grep -v "Test"')
	getPIDs(victim_pids, victim_names, 'ps -a | grep "PassThrough" | grep -v "Test"')
	#getPIDs(victim_pids, victim_names, 'ps -a | grep "ArtPot" | grep -v "Test"')
	#getPIDs(victim_pids, victim_names, 'ps -a | grep "Mapper" | grep -v "Test"')

	if (len(victim_pids) < (1 * victim_count)):
		print "Error: One of the controllers did not successfully restart"
		sys.exit()

	# Useful for something.
	#kill_num = max(victim_pids)
	#print "Executing ", cmd_start, " on", kill_num
	#Popen(cmd_start + " " + str(kill_num), shell=True)

	kill_index = random.randint(0, len(victim_pids)-1)
	print "Executing ", cmd_start, " on a ", victim_names[kill_index], ": ", victim_pids[kill_index]
	Popen(cmd_start + " " + str(victim_pids[kill_index]), shell=True)
	rep_index = (kill_index + (victim_count - 1)) % victim_count
	print "Restarting through ", victim_names[rep_index], " ", victim_pids[rep_index]
	Popen("kill -s USR1 " + str(victim_pids[rep_index]), shell=True)
