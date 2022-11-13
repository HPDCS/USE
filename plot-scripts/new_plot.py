#!/bin/python3

import sys
import numpy
import math


import matplotlib
matplotlib.use('Agg')
import numpy as np
import matplotlib.pylab as plt
from matplotlib.lines import Line2D
from scipy.signal import savgol_filter



def get_samples_from_file(filename, seconds):
    f = open(filename)
    expected = int(seconds)*2

    samples = []

    for line in f.readlines():
        line = line.strip()
        if 'thref' in line:
            line = line.split('thref')[0].split(' ')[-2]
            if 'nan' in line:
                continue
            if 'inf' in line:
                continue
            if float(line) == 0.0:
                continue
            samples += [float(line)]
        else:
            continue


    iterations = 3
    sigma = 3

    eliminated = []
    for i in range(iterations):
        if len(samples) == 0:
            print("PROBLEMATIC run:",filename)
            return [1]*expected
        avg = numpy.average(samples)
        std = numpy.std(samples)
        eliminated += [x for x in samples if x < (avg-sigma*std) ]
        eliminated += [x for x in samples if x > (avg+sigma*std) ]

        samples = [x for x in samples if x > (avg-sigma*std) ]
        samples = [x for x in samples if x < (avg+sigma*std) ]

    missing = expected - len(samples)
    parts = missing+1
    len_part = int(len(samples)/parts)
    final_sample = []

    for i in range(len(samples)):
        if len_part != 0 and i != 0 and i % len_part and missing != 0:
            final_sample += [ (final_sample[-1]+samples[i])/2]
            missing -= 1 
        final_sample += [samples[i]]

    return final_sample

#############################################

gran_to_col={
    0.25  :"darkcyan",
    0.50  :"goldenrod",
    1.0   :"indigo",
    0.4   :"indigo",
    "25" :"chocolate",
    "50" :"darkblue",
    "100":"indigo",
    "200":"darkgreen",
    "400":"darkred",
}

enfl_to_dash={
    "0":[1, 0],
    "1":[1, 1],
    "2":[2, 1],
}



seconds = 240

conversion = {
    '' : '0',
    'lo' : '1',
    'lo_re_df' : '2'
}


datafiles = {
f'pcs_lo_re_df-48-256-{seconds}-0':"2",
f'pcs_lo-48-256-{seconds}-0':"1",    
f'pcs-48-256-{seconds}-0':"0", 
f'pcs_lo_re_df-48-1024-{seconds}-0':"2",
f'pcs_lo-48-1024-{seconds}-0':"1",    
f'pcs-48-1024-{seconds}-0':"0", 
f'pcs_lo_re_df-48-4096-{seconds}-0':"2",
f'pcs_lo-48-4096-{seconds}-0':"1",    
f'pcs-48-4096-{seconds}-0':"0",  
f'pcs_hs_lo_re_df-48-256-{seconds}-0':"2",
f'pcs_hs_lo-48-256-{seconds}-0':"1",    
f'pcs_hs-48-256-{seconds}-0':"0", 
f'pcs_hs_lo_re_df-48-1024-{seconds}-0':"2",
f'pcs_hs_lo-48-1024-{seconds}-0':"1",    
f'pcs_hs-48-1024-{seconds}-0':"0", 
f'pcs_hs_lo_re_df-48-4096-{seconds}-0':"2",
f'pcs_hs_lo-48-4096-{seconds}-0':"1",    
f'pcs_hs-48-4096-{seconds}-0':"0",       
f'pcs_lo_re_df-48-256-{seconds}-1':"2",
f'pcs_lo-48-256-{seconds}-1':"1",    
f'pcs-48-256-{seconds}-1':"0", 
f'pcs_lo_re_df-48-1024-{seconds}-1':"2",
f'pcs_lo-48-1024-{seconds}-1':"1",    
f'pcs-48-1024-{seconds}-1':"0", 
f'pcs_lo_re_df-48-4096-{seconds}-1':"2",
f'pcs_lo-48-4096-{seconds}-1':"1",    
f'pcs-48-4096-{seconds}-1':"0",  
f'pcs_hs_lo_re_df-48-256-{seconds}-1':"2",
f'pcs_hs_lo-48-256-{seconds}-1':"1",    
f'pcs_hs-48-256-{seconds}-1':"0", 
f'pcs_hs_lo_re_df-48-1024-{seconds}-1':"2",
f'pcs_hs_lo-48-1024-{seconds}-1':"1",    
f'pcs_hs-48-1024-{seconds}-1':"0", 
f'pcs_hs_lo_re_df-48-4096-{seconds}-1':"2",
f'pcs_hs_lo-48-4096-{seconds}-1':"1",    
f'pcs_hs-48-4096-{seconds}-1':"0",          
}


test_list = ['pcs', 'pcs_hs']
runs =  ['0', '1', '2', '3', '4']

datafiles = {}
for test in test_list:
    for conf in ['', 'lo', 'lo_re_df']:
        for lp in ['256', '1024', '4096']:
            for r in runs:
                datafiles[f"{test}{'_' if conf != '' else ''}{conf}-48-{lp}-{seconds}-{r}"] = conversion[conf]


colors = {
'2':"darkblue",
'1':"indigo",    
'0':"darkcyan",        
}


lp_list=['256', '1024', '4096']

ti_list=[]
for i in range(5)[1:]:
    ti_list += [i*seconds/4]
#ti_list=[60,120,180,240]

if __name__ == "__main__":
    dataset = {}
    print(f"processing {sys.argv[1]}")
    for f in datafiles:
        dataset[f] = get_samples_from_file(sys.argv[1]+'/'+f, seconds)

    x_value = []
    for i in range(seconds*2):
        x_value += [0.5*i]
    tests=['pcs_', 'pcs_hs_']
    for run in runs:
        for test in tests:
            count = -1
            fig, axs = plt.subplots(1,len(lp_list), figsize = (5*len(lp_list),4), sharey=True)
            tit = f"{test[:-1]} - Platform A"
            if '0.48' in sys.argv[1]:
                tit += ' - 0.48'
            if '0.24' in sys.argv[1]:
                tit += ' - 0.24'

            fig.suptitle(tit)
            for nlp in lp_list:
                count+=1
                isFirst=True
                if len(lp_list) == 1: cur_ax = axs
                else: cur_ax = axs[count] 
                cur_ax.set_xlabel("Seconds")
                cur_ax.set_ylabel("Throughput")
                cur_ax.title.set_text(" ".join(["#cells "+nlp]))
                cur_ax.set_xticks(ti_list)
                custom_lines = []
                custom_label = []
                custom_lines = []
                custom_label = []

                min_th = 10000
                for f in dataset:
                    if f.split('-')[2] != nlp: continue
                    if f[-2:] != "-"+run : continue
                    if 'hs' not in test:
                        if 'hs' in f: continue
                    else:
                        if 'hs' not in f: continue
                        
                    cur_min = min(dataset[f])
                    if min_th > cur_min:
                        min_th = cur_min
                
                for f in dataset:
                    if f.split('-')[2] != nlp: continue
                    if f[-2:] != "-"+run : continue
                    if 'hs' not in test:
                        if 'hs' in f: continue
                    else:
                        if 'hs' not in f: continue
                        
                    enfl=datafiles[f]
                    dataplot= [x/min_th for x in dataset[f]]
                    if len(x_value) != len(dataplot):
                        print("MISSING DATAPOINTS:",f,len(x_value),len(dataplot))
                        continue
                    y_filtered = savgol_filter(dataplot, 30, 3)
                    mins = []
                    maxs = []
                    xavg = []
                    steps = 10
                    for i in range(int(len(dataplot)/steps)):
                        mins += [min(dataplot[i*steps:i*steps+steps])]
                        maxs += [max(dataplot[i*steps:i*steps+steps])]
                        xavg += [numpy.average(x_value[i*steps:i*steps+steps])]
                    #print(len(dataplot), len(y_filtered), len(maxs), len(mins))
                    #cur_ax.plot(x_value[1:], dataplot[1:], color=colors[enfl], dashes=enfl_to_dash[enfl])
                    cur_ax.plot(x_value[4:], y_filtered[4:], color=colors[enfl], dashes=enfl_to_dash[enfl])
                    cur_ax.fill_between(x=xavg,y1=mins,y2=maxs, color=colors[enfl], alpha=0.3)

                    custom_lines += [Line2D([0], [0], color=colors[enfl], dashes=enfl_to_dash[enfl])]
                    custom_label += ["el"+str(enfl)]

                #custom_lines += [Line2D([0], [0], color=gran_to_col[0.4])]
                #custom_label += ["ta=0.2"]
                
                cur_ax.legend(custom_lines, custom_label,  ncol = 3) #, bbox_to_anchor=(1.12, -0.15))
                    
            plt.savefig(f'figures/{sys.argv[2]}-{test[:-1]}-{seconds}-{run}.pdf')
