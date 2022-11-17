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
runs =  ['0', '1', '2', '3', '4', '5']

lp_list=['256', '1024', '4096']
lp_list=['4096']

datafiles = {}
for test in test_list:
    for conf in ['', 'lo', 'lo_re_df']:
        for lp in lp_list:
            for r in runs:
                datafiles[f"{test}{'_' if conf != '' else ''}{conf}-48-{lp}-{seconds}-{r}"] = conversion[conf]


colors = {
'2':"darkblue",
'1':"indigo",    
'0':"darkcyan",        
}



ta2rho={
    "0.12":r"$\rho$"+":1.00",
    "0.16":r"$\rho$"+":0.75",
    "0.24":r"$\rho$"+":0.50",
    "0.48":r"$\rho$"+":0.25",
}




ti_list=[]
for i in range(5)[1:]:
    ti_list += [i*seconds/4]
#ti_list=[60,120,180,240]

if __name__ == "__main__":
    dataset = {}
    #print(f"processing {sys.argv[1]}")
    for f in datafiles:
        dataset[f] = get_samples_from_file(sys.argv[1]+'/'+f, seconds)

    x_value = []
    for i in range(seconds*2):
        x_value += [0.5*i]
    tests=['pcs_', 'pcs_hs_']
    final = {}
    for run in runs:
        for test in tests:
            count = -1
            for nlp in lp_list:
                count+=1
                isFirst=True

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
                    dataplot= [x for x in dataset[f]]
                    dataplot= dataplot[int(len(dataplot)/2):]
                    key = '-'.join(f.split('-')[:-1])
                    if key not in final:
                        final[key] = []
                    final[key] += [numpy.average(dataplot)]
    for k in final:
        final[k] = (numpy.average(final[k]),numpy.std(final[k]))
    


    for test in ["pcs_hs"]:
        fig, axs = plt.subplots(1,len(lp_list), figsize = (5*len(lp_list),4), sharey=True)
        tit = f"{test}"
        if test == 'pcs':
            tit = 'PCS'
        else:
            tit = 'PCS with 10%/90% hot/ordinary cells'
        #if '0.48' in sys.argv[1]:
        #    tit += ' - '+ta2rho['0.48']
        #if '0.24' in sys.argv[1]:
        #    tit += ' - '+ta2rho['0.24']
        #if '0.12' in sys.argv[1]:
        #    tit += ' - '+ta2rho['0.12']

        fig.suptitle(tit)

        count = -1

        maxval = 0
        minval = 100000
        for lp in lp_list:
            count+=1
            if len(lp_list) == 1: cur_ax = axs
            else: cur_ax = axs[count] 
            #cur_ax.set_ylabel("Speedup w.r.t USE")
            cur_ax.title.set_text(" ".join(["#simulation objects:"+lp]))

            x = []
            y = []
            baseline = 0

            for k in final:
                if f"-{lp}-" not in k: continue
                if 'hs' in test and 'hs' not in k: continue
                if 'hs' not in test and 'hs' in k: continue
                name = k.replace(f'-{seconds}', '').replace('-48', '').replace(f'-{lp}', '')
                name = name.replace('pcs_hs_lo_re_df', 'el2') 
                name = name.replace('pcs_hs_lo', 'el1') 
                name = name.replace('pcs_hs', 'el0') 
                name = name.replace('pcs_lo_re_df', 'el2') 
                name = name.replace('pcs_lo', 'el1') 
                name = name.replace('pcs', 'el0') 
                if name == 'el0' : baseline = final[k][0] 
            for k in final:
                if f"-{lp}-" not in k: continue
                if 'hs' in test and 'hs' not in k: continue
                if 'hs' not in test and 'hs' in k: continue
                name = k.replace(f'-{seconds}', '').replace('-48', '').replace(f'-{lp}', '')
                name = name.replace('pcs_hs_lo_re_df', 'el2') 
                name = name.replace('pcs_hs_lo', 'el1') 
                name = name.replace('pcs_hs', 'el0') 
                name = name.replace('pcs_lo_re_df', 'el2') 
                name = name.replace('pcs_lo', 'el1') 
                name = name.replace('pcs', 'el0') 
                if name == 'el0': continue
                if name == 'el1':
                    name = 'cache opt.'
                elif name == 'el2':
                    name = 'cache + NUMA opt.'
                x += [name]
                y += [final[k][0]/baseline]
                if maxval < y[-1]: maxval = y[-1]
                if minval > y[-1]: minval = y[-1]
            
            cur_ax.bar(x,y)
            cur_ax.set_xticks(range(len(y)),x)#,rotation=90)
            cur_ax.set_yticklabels([])
            cur_ax.set_ylim([0.9*minval,maxval*1.1])
            for i in range(len(y)):
                cur_ax.annotate("{:.2f}x".format(y[i]), xy=(x[i],y[i]), ha='center', va='bottom')

        
        plt.savefig(f'figures/{sys.argv[1][:-1]}-{test}-a.pdf')

