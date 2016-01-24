# Should look for (via ps) process that need to die!
# Should find all eligible pids, then execute specified command.
# Examples - 
#  * kill a process at random - python injector.py 'kill -9'
#  * inject simulated SDC - python injector.py '/bin/kill -s SIGRTMIN+2'
#  * Use the flip bit program = python injector.py './inject_error'
# James Marshall

import random
import sys
import time
from subprocess import Popen, PIPE, call

def getPIDs(pids, weights, names, cmd):
	proc = Popen(cmd, shell=True, stdout=PIPE)
	for line in proc.stdout:
		words = line.split()
		try:
			pids.append(int(words[0]))
			weights.append(float(words[1]))
			names.append(words[2])
			# print line, "<- line | processed ->", pids[-1], weights[-1], words[-1]
		except ValueError:
			pass

if len(sys.argv) < 2:
	print "Usage: python injector.py <command_to_execute> [controller_name_0 ... controller_name_n]"
	print "\t'kill -9'\tsend SIGTERM"
	print "\t'/bin/kill -s SIGRTMIN+2'\tSimulate Silent Data Corruption"
	print "\t'/bin/kill -s SIGRTMIN+3'\tSimulate Control Flow Error"
	print "If controllers are not specified, assumes: AStar ArtPot Filter Mapper"
	sys.exit()

# This should be checked before running, but hey we are kill processes here
cmd_start = sys.argv[1]

victim_programs = []
if len(sys.argv) > 2: # victim name supplied
	for i in range(2, len(sys.argv)):
		print "Victim name is ", sys.argv[i]
		victim_programs.append(sys.argv[i])
else:
	victim_programs.append("AStar")
	victim_programs.append("Filter")
	victim_programs.append("Mapper")
	victim_programs.append("ArtPot")

print "executing: ", cmd_start

random.seed(None) # uses system time

# This will need to change if mixing TMR and DMR
victim_types = len(victim_programs) # different controllers
victim_count = 1 # 3 # replicated 3 times
while True:
	time.sleep(1/2.0)
	victim_pids = []
	victim_weights = []
	victim_names = []

	for name in victim_programs:
		# pcpu is used to weight the chance to inject base on cpu time
		search_str = 'ps -ao pid,pcpu,comm | grep "' + name + '" | grep -v "Test" | grep -v "defunct"'
		getPIDs(victim_pids, victim_weights, victim_names, search_str)

	if (len(victim_pids) < (victim_types * victim_count)):
		print "Error: One of the controllers did not successfully restart"
		#sys.exit()
	else:
		# The vicitm is selected based on victim cpu load, so the fault
		# may not be injected if load is low.
		kill_index = random.random() * 100 # * 100 to convert to percent
		psum = 0.0
		for index in range(0, len(victim_pids)):
			psum = psum + victim_weights[index]
			if psum > kill_index:
				print "Executing ", cmd_start, " on a ", victim_names[index], ": ", victim_pids[index]
				Popen(cmd_start + " " + str(victim_pids[index]), shell=True)
				break
		print "\tInjection done.\t", psum, " - ", kill_index


# These lines are only needed if there is no voter (testing) (now defunct, need update)
#rep_index = (kill_index + (victim_count - 1)) % victim_count
#print "Restarting through ", victim_names[rep_index], " ", victim_pids[rep_index]
#Popen("kill -s USR1 " + str(victim_pids[rep_index]), shell=True)
