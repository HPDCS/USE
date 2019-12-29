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

adv = []
com = []
diff = []
tot = []
eff1 = []
eff2 = []

for line in filtered:
	line = line.split(" ")
	adv.append(float(line[1].split(":")[1]))
	com.append(float(line[2].split(":")[1]))
	diff.append(float(line[3].split(":")[1]))
	tot.append(float(line[4].split(":")[1]))
	eff1.append(float(line[5].split(":")[1]))
	eff2.append(float(line[6].split(":")[1]))

PrintStats("adv",adv)


exit()

