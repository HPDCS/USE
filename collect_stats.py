# -*- coding: utf-8 -*- 

import sys
import os
import statistics

def Filter(string, substr): 
    return [str for str in string if
             substr in str] 

def PrintStats(name, array):
	avg = sum(array) / len(array)
	print name
	print "\tAvg(" + name + ")    " + str(avg)
	print "\tMax(" + name + ")    " + str(max(array)) + " ("+str(100*(max(array)-avg)/avg)+"%)"
	print "\tMin(" + name + ")    " + str(min(array)) + " ("+str(100*(min(array)-avg)/avg)+"%)"
	print "\tDevStd(" + name + ") " + str(statistics.pstdev(array)) + " ("+str(100*statistics.pstdev(array)/avg)+"%)"
	print "\tVar(" + name + ")    " + str(statistics.pvariance(array)) + " ("+str(100*statistics.pvariance(array)/avg)+"%)"
 


#controllo input comando
if len(sys.argv) < 2:
	print "Modalita' di utilizzo:"
	print "\tpython collect_stats.py execytable param1 param2"
	exit()


#Execute statically an executable with two input parameter
stream = os.popen('./'+sys.argv[1]+' '+sys.argv[2]+' '+sys.argv[3])
output = stream.read().replace("  ", " ").split("\n")

#Filter the interesting line from the input
filtered = Filter(output,"Len(ms):")

mappatura = {}
adv = []
com = []
diff = []
tot = []
eff1 = []
eff2 = []

for line in filtered:
	line = line.split(" ")
	for elem in line:
		atom = elem.split(":")
		if mappatura.has_key(atom[0]):
			stats = mappatura.get(atom[0])
		else:
			stats = []
		stats.append(float(atom[1]))
		mappatura[atom[0]] = stats

for elem in mappatura.keys():
	PrintStats(elem,mappatura.get(elem))


exit()



