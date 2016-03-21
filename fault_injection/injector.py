# Should look for (via ps) process that need to die!
# Should find all eligible pids, then execute specified command.
# Examples - See usuage message.
#
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

if len(sys.argv) < 3:
	print "Usage: python injector.py <weight?> <command_to_execute> [controller_name_0 ... controller_name_n]"
	print "\t'kill -9'\tsend SIGTERM"
	print "\t'/bin/kill -s 37'\tSimulate Silent Data Corruption (see controller.h)"
	print "\t'/bin/kill -s 38'\tSimulate Control Flow Error"
	print "\t'./inject_error'\tUse register bit flip injector (not converted to ARM, not tested recently)"
	print "If controllers are not specified, assumes: AStar ArtPot Filter Mapper"
	sys.exit()

# check if using weights
if (sys.argv[1] == "true" or sys.argv[1] == "True"):
        weighted = True
else:
        weighted = False

print "Using weights? ", weighted

# This should be checked before running, but hey we are kill processes here
cmd_start = sys.argv[2]

victim_programs = []
if len(sys.argv) > 3: # victim name supplied
	for i in range(3, len(sys.argv)):
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
victim_count = -1
while True:
	time.sleep(1/2.0)
	victim_pids = []
	victim_weights = []
	victim_names = []

	for name in victim_programs:
		# pcpu is used to weight the chance to inject base on cpu time
		search_str = 'ps -ao pid,pcpu,comm | grep "' + name + '" | grep -v "Test" | grep -v "defunct"'
		getPIDs(victim_pids, victim_weights, victim_names, search_str)

        if (-1 == victim_count): # record victim count at start, should not change.
                victim_count = len(victim_pids)

	if (len(victim_pids) < victim_count):
		print "Error: One of the controllers did not successfully restart"
		#sys.exit()
	else:
                if (weighted):
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
                else:
                        kill_index = random.randint(0, len(victim_pids)-1)
                        print "Executing ", cmd_start, " on a ", victim_names[kill_index], ": ", victim_pids[kill_index]
                        Popen(cmd_start + " " + str(victim_pids[kill_index]), shell=True)


# These lines are only needed if there is no voter (testing) (now defunct, need update)
#rep_index = (kill_index + (victim_count - 1)) % victim_count
#print "Restarting through ", victim_names[rep_index], " ", victim_pids[rep_index]
#Popen("kill -s USR1 " + str(victim_pids[rep_index]), shell=True)
