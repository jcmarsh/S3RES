# Query specified processes for rusuage info (-s 39)
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
		except ValueError:
			pass

print "Usage: python check_usage.py [controller_name_0 ... controller_name_n]"
print "If controllers are not specified, assumes: VoterM VoterD AStar ArtPot Filter Mapper. Signal sent is -s 39"

victim_programs = []
if len(sys.argv) > 1: # victim name supplied
	for i in range(1, len(sys.argv)):
		print "Victim name is ", sys.argv[i]
		victim_programs.append(sys.argv[i])
else:
	victim_programs.append("VoterM")
	victim_programs.append("VoterD")
	victim_programs.append("AStar")
	victim_programs.append("Filter")
	victim_programs.append("Mapper")
	victim_programs.append("ArtPot")

cmd_start = "kill -s 39"
print "executing: ", cmd_start

victim_pids = []
victim_weights = []
victim_names = []

for name in victim_programs:
	# pcpu is used to weight the chance to inject base on cpu time
	search_str = 'ps -ao pid,pcpu,comm | grep "' + name + '" | grep -v "Test" | grep -v "defunct"'
	getPIDs(victim_pids, victim_weights, victim_names, search_str)

for index in range(0, len(victim_pids)):
	print "Executing ", cmd_start, " on a ", victim_names[index], ": ", victim_pids[index]
	Popen(cmd_start + " " + str(victim_pids[index]), shell=True)
