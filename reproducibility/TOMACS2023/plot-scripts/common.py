import sys
import numpy
import math


import matplotlib
matplotlib.use('Agg')
import numpy as np
import matplotlib.pylab as plt
from matplotlib.lines import Line2D
from scipy.signal import savgol_filter


seconds = 120 #240
lp_list=['256', '1024', '4096']
ran = [0.65,1.35]
quantiles = [10, 90]
datafiles = {}
runs =  [str(x) for x in range(12)]


def configure_globals(test):
    global seconds
    global lp_list
    global datafiles
    global runs

    if test == 'pcs':
        pass
    else:
        seconds = 240
        lp_list = ['4096']
        runs =  [str(x) for x in range(6)]

    for test in test_list:
        for conf in ['', 'lo', 'lo_re_df']:
            for lp in lp_list:
                for r in runs:
                    datafiles[f"{test}{'_' if conf != '' else ''}{conf}-48-{lp}-{seconds}-{r}"] = conversion[conf]
                    if conf == 'lo_re_df':
                        datafiles[f"{test}{'_' if conf != '' else ''}{conf}-48-{lp}-{seconds}-{str(int(r)+6)}"] = conversion[conf]



def get_samples_from_file(filename, seconds):
    f = open(filename)
    expected = int(seconds)*2

    samples = []
    max_ts = 0

    tot_evt = 0


    for line in f.readlines():
        if 'Committed events..............................' in line:
            tot_evt =  int(line.split(' ')[-2])

        line = line.strip()
        if 'thref' in line:
            ts   = int(line.split(' ')[-1])
            line = line.split('thref')[0].split(' ')[-2]
            if ts == 0:
                continue
            if ts > max_ts:
                max_ts = ts
            if 'nan' in line:
                continue
            if 'inf' in line:
                continue
            if float(line) == 0.0:
                continue
            samples += [(float(line),ts)]
        else:
            continue


    iterations = 3
    sigma = 3

    for i in range(iterations):
        if len(samples) == 0:
            print("PROBLEMATIC run:",filename)
            return [1]*expected
        avg = numpy.average([x[0] for x in samples])
        std = numpy.std([x[0] for x in samples])

        samples = [x for x in samples if x[0] > (avg-sigma*std*3) ]
        samples = [x for x in samples if x[0] < (avg+sigma*std*3) ]

    expected_max = seconds*1000 - 500
    constant = max_ts - expected_max

    final_sample = []

    for i in range(len(samples)):
        #if samples[i][1]-constant < seconds*1000/2: continue
        if len(final_sample) == 0:
            final_sample += [(samples[i][0], samples[i][1]-constant)]
        else:
            final_sample += [(samples[i][0], samples[i][1]-constant)]
        if final_sample[-1][1] < 0:
            print("ERROR @",filename, constant, final_sample[-1], samples)
            exit()
            
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
}




conversion = {
    '' : '0',
    'lo' : '1',
    'lo_re_df' : '2'
}


test_list = ['pcs', 'pcs_hs']






                    


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

