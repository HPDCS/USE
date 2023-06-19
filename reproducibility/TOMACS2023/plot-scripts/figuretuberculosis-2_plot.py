#!/bin/python3

from common import *
import common
import os


def parse_filename(filename):

    tmp_list = filename.split('-');
    tmp_string = tmp_list[0] + "-"

    for i in range(1, len(tmp_list)):

        tmp_string += (tmp_list[i].split('_')[1])
        if (tmp_list[i] == 'threads_1'):
            tmp_string = tmp_string.replace('-1', '-seq')
        if i != len(tmp_list)-1:
            tmp_string += "-"


    #print(str(tmp_string))
    return tmp_string

def parse_back_filename(filename):

    #tuberculosis-enfl_1-numa_1-threads_40-lp_4096-run_5
    #tuberculosis-1-1-40-4096-1
    tmp_list = filename.split('-')
    tmp_string = tmp_list[0] + "-"

    tmp_string += 'enfl_' + str(tmp_list[1])
    tmp_string += '-numa_' + str(tmp_list[2])
    tmp_string += '-threads_' + str(tmp_list[3])
    tmp_string += '-lp_' + str(tmp_list[4])
    tmp_string += '-run_' + str(tmp_list[5])


    print("parsed back filename " + tmp_string)

    return tmp_string




if __name__ == "__main__":
    configure_globals(sys.argv[2])

    seconds = common.seconds
    lp_list = common.lp_list


    bp_dict = {}
    tests= [sys.argv[2]]
    ti_list=[]
    for i in range(5)[1:]:
        ti_list += [i*seconds/4]
    
    dataset = {}
    new_dataset = {}

    for f in common.datafiles:
        if os.path.isfile(sys.argv[1]+f):
            dataset[f] = get_samples_from_file(sys.argv[1]+f, seconds)
    
    x_value = []
    for i in range(seconds*2):
        x_value += [0.5*i]
    final = {}

    #print("F: " + str(f) + " dataset[f] " + str(dataset[f]))
    for f in dataset:
        tmp = f
        new_dataset[parse_filename(f)] = dataset[tmp]

    #print("new dataset: " + str(new_dataset))

    #tuberculosis-enfl_1-numa_1-threads_40-lp_4096-run_5
    #tuberculosis-1-1-40-4096-1

    for run in range(1, 6):
        for test in tests:
            count = -1
            fig, axs = plt.subplots(1,len(lp_list), figsize = (5*len(lp_list),4), sharey=True)
            tit = f"{test}"
            if test == 'tuberculosis':
                tit = 'TUBERCULOSIS'

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

                min_th = 100000
                for f in new_dataset:

                    #print("f: " + str(f) + " f[-2:] " + str(f[-2:]))
                    if f.split('-')[4] != nlp: continue
                    if f[-2:] != "-"+str(run) : continue
                    

                    last_samples = new_dataset[f][0]
                    #print("last_samples " + str(last_samples))
                    offset = -len(last_samples)/10
                    last_samples = last_samples[int(offset):]
                    last_list = []
                    for i in range(len(last_samples)):
                        if i == 0: continue
                        last_list += [last_samples[i][0]*(last_samples[i][1]-last_samples[i-1][1])]

                    print("last list " + str(last_list))
                    min_th = sum(last_list)/(last_samples[-1][1]-last_samples[0][1])
                    #print(min_th)

                for f in new_dataset:
                    if 'seq' in f: continue
                    if f.split('-')[4] != nlp: continue
                    if f[-2:] != "-"+str(run) : continue
                    
                    print("F in new_dataset " + (str(f)))

                    original_filename = parse_back_filename(f)

                    print("F in dataset " + str(original_filename))

                    enfl=datafiles[original_filename]
                    x_value = [x[1]/1000 for x in dataset[original_filename][0]]
                    dataplot= [x[0]/min_th for x in dataset[original_filename][0]]

                    name = f.replace('tuberculosis-', '').replace('-40-', '-').replace(f'-{nlp}', '').replace(f'-{run}', '')
                    name = name.replace('0-0', 'el0')
                    name = name.replace('1-0', 'el1')
                    name = name.replace('1-1', 'el2')

                    print("NAME " + str(name))

                    y_filtered = savgol_filter(dataplot, 11, 3)
                    mins = []
                    maxs = []
                    xavg = []
                    steps = 2
                    for i in range(int(len(dataplot)/steps)):
                        mins += [min(dataplot[i*steps:i*steps+steps])]
                        maxs += [max(dataplot[i*steps:i*steps+steps])]
                        xavg += [numpy.average(x_value[i*steps:i*steps+steps])]
                    #print(len(dataplot), len(y_filtered), len(maxs), len(mins))

                    if name == 'el1':
                        name = 'cache\nopt.'
                        enfl = '1'
                    elif name == 'el2':
                        name = 'cache\n+ NUMA opt.'
                        enfl = '2'
                    elif name == 'el0':
                        name = 'baseline'
                        enfl = '0'

                    #cur_ax.plot(x_value[1:], dataplot[1:], color=colors[enfl], dashes=enfl_to_dash[enfl])
                    cur_ax.plot(x_value, y_filtered, color=colors[enfl], dashes=enfl_to_dash[enfl])
                    cur_ax.fill_between(x=xavg,y1=mins,y2=maxs, color=colors[enfl], alpha=0.3)

                    custom_lines += [Line2D([0], [0], color=colors[enfl], dashes=enfl_to_dash[enfl])]
                    if enfl == '0':
                        custom_label += ['USE']
                    elif enfl == '1':
                        custom_label += ['cache opt.']
                    elif enfl == '2':    
                        custom_label += ['cache + NUMA opt.']

                cur_ax.legend(custom_lines, custom_label,  ncol = 1) #, bbox_to_anchor=(1.12, -0.15))

    

        
        plt.savefig(f'figures/{sys.argv[1][:-1].replace("/", "-")}-{test}-240s_2 .pdf')

