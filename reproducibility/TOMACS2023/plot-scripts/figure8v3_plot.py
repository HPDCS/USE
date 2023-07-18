#!/bin/python3

from common import *
import common
import os


if __name__ == "__main__":
    configure_globals(sys.argv[2])

    seconds = common.seconds
    lp_list = common.lp_list

    bp_dict = {}
    tests= [sys.argv[2]]
    
    dataset = {}
    

    for f in common.datafiles:
        if os.path.isfile(sys.argv[1]+'/'+f):
            dataset[f] = get_samples_from_file(sys.argv[1]+'/'+f, seconds)
    

    x_value = []
    for i in range(seconds*2):
        x_value += [0.5*i]
    final = {}


    for test in tests:
        for nlp in lp_list:
            isFirst=True
            for f in dataset:
                if f.split('-')[2] != nlp: continue
                #if f[-2:] != "-"+run : continue
                if 'hs' not in test:
                    if 'hs' in f: continue
                else:
                    if 'hs' not in f: continue
                
                enfl=datafiles[f]
                res,tot_evt = dataset[f]
                dataset[f] = res
                dataplot = []
                for i in range(len(dataset[f])):
                    #if dataset[f][i][1]<1000*seconds/2: continue
                    if len(dataplot) == 0:
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
        final[k] = sorted(final[k])
        final[k] = final[k][1:-1]
        bp_dict[k] = {
        'med': numpy.median(final[k])           ,
        'mean': numpy.average(final[k])           ,        
        'q1':  numpy.percentile(final[k], quantiles[0]),
        'q3':  numpy.percentile(final[k], quantiles[1]),
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
            tit = 'PCS with 20%/ 80% hot/ordinary cells'

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
            #cur_ax.set_ylabel("Speedup w.r.t USE")
            cur_ax.set_ylabel("Throughput ($10^6$ evts per sec)")
            cur_ax.title.set_text(" ".join(["#simulation objects:"+lp]))

            baseline = 0
            x = []
            y = []
            cur_bp = []
            for k in final:
                if f"-{lp}-" not in k: continue
                if 'hs' in test and 'hs' not in k: continue
                if 'hs' not in test and 'hs' in k: continue
                #print(k)
                name = k.replace(f'-{seconds}', '').replace('-40-', '-').replace(f'-{lp}', '')
                name = name.replace('pcs_hs_lo_re_df', 'el2') 
                name = name.replace('pcs_hs_lo', 'el1') 
                name = name.replace('pcs_hs', 'el0') 
                name = name.replace('pcs_lo_re_df', 'el2') 
                name = name.replace('pcs_lo', 'el1') 
                name = name.replace('pcs', 'el0')
                #print(name) 
                if name == 'seq-1' : 
                    #print(k)
                    baseline = final[k][0] 
                    y += [baseline]
                    #print(baseline)
                    bp_dict[k]['label'] = 'sequential'
                    cur_bp += [bp_dict[k].copy()]
                if name == 'seq_hs-1' : 
                    #print(k)
                    baseline = final[k][0] 
                    y += [baseline]
                    #print(baseline)
                    bp_dict[k]['label'] = 'sequential'
                    cur_bp += [bp_dict[k].copy()]
            for k in final:
                if f"-{lp}-" not in k: continue
                if 'hs' in test and 'hs' not in k: continue
                if 'hs' not in test and 'hs' in k: continue
                name = k.replace(f'-{seconds}', '').replace('-40-', '-').replace(f'-{lp}', '')
                name = name.replace('pcs_hs_lo_re_df', 'el2') 
                name = name.replace('pcs_hs_lo', 'el1') 
                name = name.replace('pcs_hs', 'el0') 
                name = name.replace('pcs_lo_re_df', 'el2') 
                name = name.replace('pcs_lo', 'el1') 
                name = name.replace('pcs', 'el0') 
                if name == 'seq-1': continue
                if name == 'seq_hs-1': continue
                if name == 'el1':
                    name = 'cache\nopt.'
                elif name == 'el2':
                    name = 'cache\n+ NUMA opt.'
                elif name == 'el0':
                    name = 'baseline'
                x += [name]
                y += [final[k][0]]
                bp_dict[k]['label'] = name
                cur_bp += [bp_dict[k].copy()]
                if maxval < y[-1]: maxval = y[-1]
                if minval > y[-1]: minval = y[-1]
            #print("BASELINE", baseline, lp)
            for d in cur_bp:
                #print(d)
                for v in d:
                    #print(v, d[v])
                    if v != 'label':
                        d[v] = d[v] #/baseline

            cur_ax.bxp(cur_bp, showfliers=False, showmeans=True)
            #cur_ax.set_ylim(ran)
            cur_ax.yaxis.grid()
            #cur_ax.yaxis.set_ticks(np.arange(ran[0], ran[1], 0.05))
            #cur_ax.set_ylabel("Speedup w.r.t USE")

        
        plt.savefig(f'figures/{sys.argv[1][:-1].replace("/", "-")}-{test}-half.pdf')

