#!/bin/python3

from common import *
import os


tests= [sys.argv[2]]
ti_list=[]
for i in range(5)[1:]:
    ti_list += [i*seconds/4]


if __name__ == "__main__":
    dataset = {}
    #print(f"processing {sys.argv[1]}")
    for f in datafiles:
        if os.path.isfile(sys.argv[1]+'/'+f):
            dataset[f] = get_samples_from_file(sys.argv[1]+'/'+f, seconds)
        else:
            print(f"file {sys.argv[1]+'/'+f} not found...still trying")

    x_value = []
    for i in range(seconds*2):
        x_value += [0.5*i]
    
    for run in runs:
        for test in tests:
            count = -1
            fig, axs = plt.subplots(1,len(lp_list), figsize = (5*len(lp_list),4), sharey=True)
            tit = f"{test}"
            if test == 'pcs':
                tit = 'PCS'
            else:
                tit = 'PCS with 10%/90% hot/ordinary cells'
            if '0.48' in sys.argv[1]:
                tit += ' - '+ta2rho['0.48']
            if '0.24' in sys.argv[1]:
                tit += ' - '+ta2rho['0.24']
            if '0.12' in sys.argv[1]:
                tit += ' - '+ta2rho['0.12']

            fig.suptitle(tit)
            for nlp in lp_list:
                count+=1
                isFirst=True
                if len(lp_list) == 1: cur_ax = axs
                else: cur_ax = axs[count] 
                cur_ax.set_xlabel("Seconds")
                cur_ax.set_ylabel("Throughput")
                cur_ax.title.set_text(" ".join(["#simulation objects:"+nlp]))
                cur_ax.set_xticks(ti_list)
                #cur_ax.set_ylim([0,0.75])
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
                    
                    if "pcs_hs-" not in f and "pcs-" not in f: continue

                    min_th = numpy.average(dataset[f][100:])
                    #print(min_th)
                    
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
                    y_filtered = savgol_filter(dataplot, 4, 3)
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
                    if enfl == '0':
                        custom_label += ['USE']
                    elif enfl == '1':
                        custom_label += ['cache opt.']
                    elif enfl == '2':    
                        custom_label += ['cache + NUMA opt.']
                cur_ax.legend(custom_lines, custom_label,  ncol = 1) #, bbox_to_anchor=(1.12, -0.15))
                    
            plt.savefig(f'figures/{sys.argv[1][:-1]}-{test}-{seconds}-{run}.pdf')