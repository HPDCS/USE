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


test_list = ['pcs', 'pcs_hs']

runs =  ['0', '1', '2', '3', '4', '5']



lp_list=['256', '1024', '4096']

datafiles = {}
for test in test_list:
    for conf in ['', 'lo', 'lo_re_df']:
        for lp in lp_list:
            for r in runs:
                datafiles[f"{test}{'_' if conf != '' else ''}{conf}-48-{lp}-{seconds}-{r}"] = conversion[conf]
                if conf == 'lo_re_df':
                    datafiles[f"{test}{'_' if conf != '' else ''}{conf}-48-{lp}-{seconds}-{str(int(r)+6)}"] = conversion[conf]
                    


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

