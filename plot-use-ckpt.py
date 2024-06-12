
import os
import sys
import numpy
import math


import matplotlib
matplotlib.use('Agg')
import numpy as np
import matplotlib.pylab as plt
from matplotlib.lines import Line2D


inputDir = "./results/"

thread_list = [40]
ckpt_list = []

mprot_list = [] #mprotect
klm_list = [] #lkm


def parse_list(file_list, check_str, pages_str):
# @param file_list: list of filenames
# @param check_str: string to be checked for filling the list
# @param page_str: string of page number

	for filename in os.listdir(inputDir):
	    full_path = os.path.join(inputDir, filename)
	    print(filename)
	    if filename.find(check_str) != -1:
	    	line = filename.split('-')
	    	ckpt = line[3].split('.')
	    	print(ckpt[0])
	    	if (pages_str == '1'):
	    		ckpt_list.append(int(ckpt[0]))
	    	file_list.append(filename)

    

parse_list(mprot_list, 'mprotect', '1')
parse_list(klm_list, 'lkm', '0')
#parse_list(klm_smallmem_list, 'compile-1_prot-1', 'pages-1')
#parse_list(klm_largemem_list, 'compile-1_prot-1', 'pages-512')

ckpt_list.sort()
print(ckpt_list)

mprot_list = sorted(mprot_list, key=lambda x: int(x.split('-')[3].split('.')[0]))
print(mprot_list)

klm_list = sorted(klm_list, key=lambda x: int(x.split('-')[3].split('.')[0]))
print(klm_list)


def convert_in_seconds(mode, line):
# Convert each time in the file to seconds
	minutes, seconds = line.split('m')
	minutes = int(minutes)
	seconds = float(seconds.rstrip('s'))
	total_seconds = minutes * 60 + seconds
	#print("total seconds for line " + mode + " : " + str(total_seconds))
	return total_seconds


def read_data(file_list, output_list, mode):
# @param file_list: list of filenames to read
# @param output_list : list to fill with time data
# @param mode: string 'real' 'user' or 'sys' relative to time output
	for filename in file_list:
		f = open(inputDir + filename)
		print(filename)
		for line in f:
			line = line.strip()
			if line == '':
				continue
			#line = line.split('\t')
			if line.find("EventsPerSec") != -1:
				line = line.split(':')
				stri = line[1].replace(" ", '')
				print(stri)
				output_list.append(stri)
			#print(line[1])


####### BELOW ONLY MPROTECT 1 PAGE

data_mprot = []

data_klm = []




read_data(mprot_list, data_mprot, 'real')
read_data(klm_list, data_klm, 'real')


print("evts per sec mprotect "  + str(data_mprot))
print(' ------ ')
print("evts per sec lkm " + str(data_klm))


fig = plt.figure(figsize=(10,10))
#ax = fig.add_subplot(211)
#ax.set_yscale('log')

plt.title("Throughput of PCS ran with 40 threads and 256 simulation objects")
plt.plot(ckpt_list, data_mprot,  marker="X", color='red', label="Incremental State Saving via mprotect")
plt.plot(ckpt_list, data_klm, marker="s", label="Incremental State Saving via LKM facilities")
plt.legend()
plt.xlabel("Checkpoint Period")
plt.ylabel("Throughput (Committed Events Per Second)")

plt.savefig('./figure-ckpt-periods-log.pdf')

plt.clf()


