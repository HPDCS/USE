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
    #    else:
    #        print(f"file {sys.argv[1]+'/'+f} not found...still trying")

    x_value = []
    for i in range(seconds*2):
        x_value += [0.5*i]
    final = {}
    #for run in runs:
    for test in tests:
        count = -1
        for nlp in lp_list:
            count+=1
            isFirst=True

            for f in dataset:
                if f.split('-')[2] != nlp: continue
                #if f[-2:] != "-"+run : continue
                if 'hs' not in test:
                    if 'hs' in f: continue
                else:
                    if 'hs' not in f: continue
                
                enfl=datafiles[f]
                dataplot = []
                for i in range(len(dataset[f])):
                    if i == 0:
                        dataplot += [dataset[f][i][0]*dataset[f][i][1]]
                    else:
                        dataplot += [dataset[f][i][0]*(dataset[f][i][1]-dataset[f][i-1][1])]
                        
                avg = sum(dataplot)/seconds/1000
                if(avg < 0):
                    print(f,dataplot)
                    exit()
                #dataplot= dataplot[int(len(dataplot)/2):]
                key = '-'.join(f.split('-')[:-1])
                if key not in final:
                    final[key] = []
                final[key] += [avg]
                

    for k in final:
        final[k] = (numpy.average(final[k]),numpy.std(final[k]))

    for test in tests:
        fig, axs = plt.subplots(1,len(lp_list), figsize = (5*len(lp_list),4), sharey=True)
        tit = f"{test}"
        if test == 'pcs':
            tit = 'PCS'
        else:
            tit = 'PCS with 10%/ 90% hot/ordinary cells'

        if '0.48' in sys.argv[1]:
            tit += ' - '+ta2rho['0.48']
        if '0.24' in sys.argv[1]:
            tit += ' - '+ta2rho['0.24']
        if '0.12' in sys.argv[1]:
            tit += ' - '+ta2rho['0.12']

        fig.suptitle(tit)

        count = -1

        maxval = 0
        minval = 100000
        for lp in lp_list:
            count+=1
            if len(lp_list) == 1: cur_ax = axs
            else: cur_ax = axs[count] 
            cur_ax.set_ylabel("Speedup w.r.t USE")
            cur_ax.title.set_text(" ".join(["#simulation objects:"+lp]))

            baseline = 0
            x = []
            y = []
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
            
            x = ['baseline']+x
            y = [1] + y
            cur_ax.bar(x,y)
            cur_ax.set_xticks(range(len(y)),x)#,rotation=90)
            cur_ax.set_yticklabels([])
            cur_ax.set_ylim([0,1.5])
            for i in range(len(y)):
                cur_ax.annotate("{:.2f}x".format(y[i]), xy=(x[i],y[i]), ha='center', va='bottom')

        
        plt.savefig(f'figures/{sys.argv[1][:-1].replace("/", "-")}-{test}.pdf')

