#!/bin/python3

from common import *
import os

bp_dict = {}
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
    for run in runs:
        for test in tests:
            for nlp in lp_list:
                isFirst=True
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
        bp_dict[k] = {
        'med': numpy.median(final[k])           ,
        'mean': numpy.average(final[k])           ,
        'q1':  numpy.percentile(final[k], 25),
        'q3':  numpy.percentile(final[k], 75),
        'whislo': min(final[k]),
        'whishi': max(final[k])
        }
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
            cur_bp = []
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
                if name == 'el0' : 
                    #print(k)
                    baseline = final[k][0] 
                    y += [baseline]
                    bp_dict[k]['label'] = 'baseline'
                    cur_bp += [bp_dict[k].copy()]
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
                y += [final[k][0]]
                bp_dict[k]['label'] = name
                cur_bp += [bp_dict[k].copy()]
                if maxval < y[-1]: maxval = y[-1]
                if minval > y[-1]: minval = y[-1]
            
            for d in cur_bp:
                #print(d)
                for v in d:
                    #print(v, d[v])
                    if v != 'label':
                        d[v] = d[v]/baseline

            cur_ax.bxp(cur_bp, showfliers=False, showmeans=True)
            ran = [0.70,1.30]
            cur_ax.set_ylim(ran)
            cur_ax.yaxis.grid()
            cur_ax.yaxis.set_ticks(np.arange(ran[0], ran[1], 0.05))
            #cur_ax.set_ylabel("Speedup w.r.t USE")

        
        plt.savefig(f'figures/{sys.argv[1][:-1].replace("/", "-")}-{test}-half.pdf')

