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

if len(sys.argv) < 1:
	print "please supply command to execute on unspecting processes (for example, 'kill -9' or 'kill -s USR2'"

# This should be checked before running, but hey we are kill processes here
cmd_start = sys.argv[1]

while True:
	time.sleep(1)
	victim_pids = []
	victim_names = []

	getPIDs(victim_pids, victim_names, 'ps -a | grep "AStar"')
	getPIDs(victim_pids, victim_names, 'ps -a | grep "Filter"')
	getPIDs(victim_pids, victim_names, 'ps -a | grep "ArtPot"')
	getPIDs(victim_pids, victim_names, 'ps -a | grep "Mapper"')

	kill_index = random.randint(0, len(victim_pids)-1)

	print "Executing ", cmd_start, " on a ", victim_names[kill_index], ": ", victim_pids[kill_index]

	Popen(cmd_start + " " + str(victim_pids[kill_index]), shell=True)
