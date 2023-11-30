#!/bin/python3

from commonF11d import *
import os
import commonF11d as common


if __name__ == "__main__":
    configure_globals(sys.argv[2])
    dataset = {}
    seconds = common.seconds
    lp_list = common.lp_list
    
    tests= [sys.argv[2]]
    ti_list=[]
    for i in range(8)[:]:
        ti_list += [i*60]

    #print(f"processing {sys.argv[1]}")
    for f in common.datafiles:
        if os.path.isfile(sys.argv[1]+'/'+f):
            dataset[f] = get_samples_from_file(sys.argv[1]+'/'+f, seconds)
    #    else:
    #        print(f"file {sys.argv[1]+'/'+f} not found...still trying")
    
    all_c = 0
    f_data = []
    for run in common.runs:
        for test in tests:
            count = -1
            all_c += 1
            if all_c == 1:
                fig, axs = plt.subplots(1,len(lp_list), figsize = (5*len(lp_list),4), sharey=True)
            tit = f"{test}"
            if test == 'pcs':
                tit = 'PCS'
            else:
                tit = 'PCS with 20%/80% hot/ordinary cells'
            if '0.4' in sys.argv[1]:
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
                cur_ax.set_ylabel("#Migrations")
                cur_ax.title.set_text(" ".join(["#simulation objects:"+nlp]))
                cur_ax.set_xticks(ti_list)
                cur_ax.set_ylim([-10,210])
                custom_lines = []
                custom_label = []
                custom_lines = []
                custom_label = []

                min_th = 100000
                for f in dataset:
                    fn = f.replace(".dat", "")
                    if f.split('-')[2] != nlp: continue
                    if fn[-2:] != "-"+run : continue
                    if 'hs' not in test:
                        if 'hs' in f: continue
                    else:
                        if 'hs' not in f: continue
                    
                    if "pcs_hs-" not in f and "pcs-" not in f: continue

                    last_samples = dataset[f][0]
                    offset = -len(last_samples)/10
                    last_samples = last_samples[int(offset):]
                    last_list = []
                    for i in range(len(last_samples)):
                        if i == 0: continue
                        last_list += [last_samples[i][0]*(last_samples[i][1]-last_samples[i-1][1])]

                    min_th = sum(last_list)/(last_samples[-1][1]-last_samples[0][1])
                    #print(min_th)
                    
                for f in dataset:
                    fn = f.replace(".dat", "")
                    if 'seq' in f: continue
                    if f.split('-')[2] != nlp: continue
                    if fn[-2:] != "-"+run : continue
                    if 'hs' not in test:
                        if 'hs' in f: continue
                    else:
                        if 'hs' not in f: continue
                        
                    enfl=datafiles[f]
                    x_value = [x[1]/1000 for x in dataset[f][0]]
                    dataplot= [x[0] for x in dataset[f][0]]

                    y_filtered = dataplot[:] #savgol_filter(dataplot, 10, 3)
                    f_data += [[[0]+x_value[1:], [0]+dataplot[1:]]]
                    mins = []
                    maxs = []
                    xavg = []
                    steps = 2
                    for i in range(int(len(dataplot)/steps)):
                        mins += [min(dataplot[i*steps:i*steps+steps])]
                        maxs += [max(dataplot[i*steps:i*steps+steps])]
                        xavg += [numpy.average(x_value[i*steps:i*steps+steps])]
                    #print(len(dataplot), len(y_filtered), len(maxs), len(mins))
                    #cur_ax.plot(x_value[1:], dataplot[1:], color=colors[enfl], dashes=enfl_to_dash[enfl])
                    
                    #cur_ax.plot(x_value, y_filtered, color=colors[enfl], dashes=enfl_to_dash[enfl])
                    #cur_ax.fill_between(x=xavg,y1=mins,y2=maxs, color=colors[enfl], alpha=0.3)

                    #custom_lines += [Line2D([0], [0], color=colors[enfl], dashes=enfl_to_dash[enfl])]
                    #if enfl == '0':
                    #    custom_label += ['USE']
                    #elif enfl == '1':
                    #    custom_label += ['cache opt.']
                    #elif enfl == '2':    
                    #    custom_label += ['cache + NUMA opt.']
    f_data_dict = []
    for d in f_data:
        run = {}
        f_data_dict += [run]
        for i in range(len(d[0])):
            ts = int(d[0][i])
            v  = d[1][i]
            if ts not in run: run[ts] = 0
            run[ts] = max(run[ts], v)

    x_value = range(421)

    
    for i in x_value:
        for r in f_data_dict:
            if i not in r: r[i] = r[i-1]

    final_lists = []
    for i in x_value:
        l = []
        for r in f_data_dict:
            l += [r[i]]
        final_lists += [l]

    mins = []
    maxs = []
    xavg = []
    q25 = []
    q75 = []
    for i in x_value:
        mins += [min(final_lists[i])]
        maxs += [max(final_lists[i])]
        q25  += [numpy.percentile(final_lists[i], 25)]
        q75  += [numpy.percentile(final_lists[i], 75)]
        xavg += [numpy.average(final_lists[i])]
    
    cur_ax.plot(x_value, xavg, color=colors[enfl], dashes=enfl_to_dash[enfl])
    cur_ax.plot(x_value, mins, color='red',   dashes=enfl_to_dash[enfl])
    cur_ax.plot(x_value, maxs, color='green', dashes=enfl_to_dash[enfl])
    #cur_ax.fill_between(x=x_value,y1=mins,y2=maxs, color=colors[enfl], alpha=0.3)
    custom_lines += [Line2D([0], [0], color=colors[enfl], dashes=enfl_to_dash[enfl])]
    custom_lines += [Line2D([0], [0], color='red',   dashes=enfl_to_dash[enfl])]
    custom_lines += [Line2D([0], [0], color='green', dashes=enfl_to_dash[enfl])]

    custom_label += ['avg']
    custom_label += ['min']
    custom_label += ['max']

    cur_ax.fill_between(x=x_value,y1=q25,y2=q75, color=colors[enfl], alpha=0.3)

    cur_ax.legend(custom_lines, custom_label,  ncol = 1, loc='center right') #, bbox_to_anchor=(1.12, -0.15))
    cur_ax.yaxis.grid()
    plt.savefig(f'figures_reproduced/figure11d-{sys.argv[1][:-1].replace("/", "-")}-{test}-{seconds}-mcnt.pdf')
