import sys
import numpy
import math


import matplotlib
matplotlib.use('Agg')
import numpy as np
import matplotlib.pylab as plt
from matplotlib.lines import Line2D
from scipy.signal import savgol_filter


seconds = 120
lp_list=['256', '1024', '4096']
ran = [0.8,1.8]
quantiles = [25, 75]
#quantiles = [50.0-12.5, 50.0+12.5]
datafiles = {}
runs =  [str(x) for x in range(12)]
max_threads='10'

for line in open("thread.conf"):
    if "MAX_THREADS" in line:
        line = line.split("=")[-1].split("#")[0].replace('"', '').strip().split(' ')
        #print(line)
        max_threads = str(max([int(x) for x in line]))

enfl_list=['0', '1']
numa_list=['0', '1']
all_samples = True

def configure_globals(test, full):
    global seconds
    global lp_list
    global datafiles
    global runs
    global ran
    global all_samples

    all_samples = full

    if test == 'tuberculosis':
        seconds = 60
        lp_list=['1024']
        for lp in lp_list:
            for r in range(12):
                datafiles[f"{test}-enfl_0-numa_0-threads_1-lp_{lp}-run_{r}"] = '1'


    if test == 'tuberculosis':
        for enf in enfl_list:
            for n in numa_list:
                if enf == 0 and n == 1: 
                    continue
                for lp in lp_list:
                    for r in range(12):
                        datafiles[f"{test}-enfl_{enf}-numa_{n}-threads_{max_threads}-lp_{lp}-run_{r}"] = '0'


    if test != 'tuberculosis':
        ftest = test
        if test == 'pcs':
            pass
        else:
            seconds = 240
            lp_list=['4096']
            if 'dyn' in test: 
              seconds = 420
            ftest = 'pcs_hs'
            
            lp_list = ['4096']
            runs =  [str(x) for x in range(6)]

        if test != 'tuberculosis':
            for lp in lp_list:
                for r in runs:
                    for conf in ['', 'lo', 'lo_re_df']:
                        datafiles[f"{ftest}{'_' if conf != '' else ''}{conf}-{max_threads}-{lp}-{seconds}-{r}.dat"] = conversion[conf]
                        if conf == 'lo_re_df':
                            datafiles[f"{ftest}{'_' if conf != '' else ''}{conf}-{max_threads}-{lp}-{seconds}-{str(int(r))}.dat"] = conversion[conf]
                    if test == 'pcs':
                        datafiles[f"seq-1-{lp}-{seconds}-{str(int(r))}.dat"] = '3'
                    else:
                        datafiles[f"seq_hs-1-{lp}-{seconds}-{str(int(r))}.dat"] = '3'
                


def get_samples_from_file(filename, seconds):
    f = open(filename)
    expected = int(seconds)*2

    samples = []
    max_ts = 0

    tot_evt = 0
    cnt = 0
    if "_lo" not in filename and "-enfl_" not in filename:
        cnt += 1

    overall_samples = 0

    for line in f.readlines():
        if 'Committed events..............................' in line:
            tot_evt =  int(line.split(' ')[-2])

        line = line.strip()
        #print("LINE " + str(line))
        if 'thref' in line:
            if cnt == 0:
                cnt+=1
                continue
            #print(line)
            #print(filename)
            ts   = int(line.split(' ')[-5])
            line = line.split('thref')[0].split(' ')[-3:-1]
            #print(line)
            line = line[-1]
            overall_samples += 1
            
            if ts == 0:
                continue
            if ts > max_ts:
                max_ts = ts
            if 'nan' in line:
                if len(samples) > 0:
                  line = samples[-1][0]
                else: continue
            elif 'inf' in line:
                #continue
                if len(samples) > 0:
                  line = samples[-1][0]
                else: continue
            elif float(line) == 0.0:
                if len(samples) == 0: continue
                line = samples[-1][0]
            #print(float(line),ts)
            line = float(line)
            if len(samples) > 0  and (line < samples[-1][0]/50.0 or line > samples[-1][0]*40.0):
               line = samples[-1][0]
            samples += [(line,ts)]
        else:
            continue

    #print("overall:",overall_samples)
    #print("filter:",len(samples))
    iterations = 2
    sigma = 2.5

    final_sample = []
    expected_max = seconds*1000 - 500

    constant = max_ts - expected_max

    for i in range(len(samples)):
        ts = samples[i][1]-constant
        if not all_samples and ts < seconds*1000/2: continue
        if not all_samples: ts-=(seconds/2)*1000
        if len(final_sample) == 0:
            final_sample += [(samples[i][0], ts)]
        else:
            final_sample += [(samples[i][0], ts)]
        #print("final sample " + str(final_sample))
        if final_sample[-1][1] < 0:
            print("ERROR @",filename, constant, final_sample[-1], samples, final_sample)
            exit()

    for i in range(iterations):
        if len(final_sample) == 0:
            print("PROBLEMATIC run:",filename)
            return [1]*expected
        values = [x[0] for x in final_sample]
        avg = numpy.average(values)
        std = numpy.std(values)

        final_sample = [x for x in final_sample if x[0] >= (avg-sigma*std) ]
        final_sample = [x for x in final_sample if x[0] <= (avg+sigma*std) ]

    max_sampl = max([x[0] for x in final_sample])
    #final_sample = [x for x in final_sample if x[0] >= (max_sampl*0.60) ]
    

    #for i in range(iterations):
    #    if len(final_sample) == 0:
    #        print("PROBLEMATIC run:",filename)
    #        return [1]*expected
    #    values = [x[0] for x in final_sample]
    #    avg = numpy.average(values)
    #    std = numpy.std(values)
    #    
    #    final_sample = [x for x in final_sample if x[0] >= (avg-sigma*std) ]
    #    final_sample = [x for x in final_sample if x[0] <= (avg+sigma*std) ]

    #for s in samples:
    # print(s)
            
    return final_sample,tot_evt

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
    "3":[3, 1],
}




conversion = {
    '' : '0',
    'lo' : '1',
    'lo_re_df' : '2'
}


test_list = ['pcs', 'pcs_hs']


colors = {
'3':"darkred",
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


