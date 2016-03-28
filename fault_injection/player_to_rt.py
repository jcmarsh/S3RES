# Search for player's pid, and then set it to an RT priority.
#
# James Marshall

import sys
import time
from subprocess import Popen, PIPE, call

# Get the pid of the named process
def getPID(name):
	proc = Popen('ps | grep "' + name + '"', shell=True, stdout=PIPE)
	for line in proc.stdout:
		words = line.split()
		try:
			pid = words[0]
			print line, "<- line | processed ->", pid
		except ValueError:
			pass
		return pid

if len(sys.argv) < 2:
	print "Usage: python player_to_rt.py <rt_priority>"
	print "\tTry a priority of 30."
	sys.exit()

priority = sys.argv[1]
print "priority", priority

pid = getPID("player")

Popen("chrt -r -p " + priority + " " + pid, shell=True)
