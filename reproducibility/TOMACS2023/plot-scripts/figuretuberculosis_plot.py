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


if __name__ == "__main__":
    configure_globals(sys.argv[2])

    seconds = common.seconds
    lp_list = common.lp_list


    bp_dict = {}
    tests= [sys.argv[2]]
    
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

    for test in tests:
        for nlp in lp_list:
            isFirst=True
            for f in new_dataset:

                #print("f ALL'INIZIO: " + str(f) + " --- " + str(f[4]))
                if f.split('-')[4] != nlp: continue
                #if f[-2:] != "-"+run : continue
                #print("f in dataset " + str(f) + " with lps " + str(nlp))

                enfl=new_dataset[f]
                res,tot_evt = new_dataset[f]
                new_dataset[f] = res
                dataplot = []

                #print("stringa: " + str(f) + " res: " + str(res))

                for i in range(len(new_dataset[f])):
                    #if dataset[f][i][1]<1000*seconds/2: continue
                    #print("dataset: " + str(new_dataset[f][i][0]) + "---- " + str(new_dataset[f][i][1]))
                    if len(dataplot) == 0:
                        dataplot += [new_dataset[f][i][0]*new_dataset[f][i][1]]
                    else:
                        dataplot += [new_dataset[f][i][0]*(new_dataset[f][i][1]-new_dataset[f][i-1][1])]

                #print("dataplot: " + str(dataplot))
                        
                avg = sum(dataplot)/seconds/1000
                if(avg < 0):
                    print(f,dataplot)
                    exit()
                #dataplot= dataplot[int(len(dataplot)/2):]
                #print("AAAA " + str(f.split('-')[:-1]))
                key = '-'.join(f.split('-')[:-1])
                #print("key: " + str(key))
                if key not in final:
                    final[key] = []
                final[key] += [avg]


    #forma: enfl-numa-lps:[run1, run2, run3, run4, run5]
    #print("FINAL " + str(final))


    for k in final:
        final[k] = sorted(final[k])
        final[k] = final[k][1:-1]
        #print("final k: " + str(k))
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
    
    print("FINAL AFTER" + str(final) + "len : " + str(len(final)))


    #for k in final:
    #    if '4096' not in k: continue
    #    if k not in portion:
    #        portion[k] = ()
     #   portion[k] += final[k]

    #print("portion: " + str(portion))

    for test in tests:
        fig, axs = plt.subplots(1,len(lp_list), figsize = (5*len(lp_list),4), sharey=True)
        tit = f"{test}"
        if test == 'tuberculosis':
            tit = 'TUBERCULOSIS'


        fig.suptitle(tit)

        count = -1
        maxval = 0
        minval = 100000
        for lp in lp_list:
            count+=1
            if len(lp_list) == 1: cur_ax = axs
            else: cur_ax = axs[count] 
            #cur_ax.set_ylabel("Speedup w.r.t USE")
            cur_ax.set_ylabel("Throughput")
            cur_ax.title.set_text(" ".join(["#simulation objects:"+lp]))

            baseline = 0
            x = []
            y = []
            cur_bp = []

            for k in final:
                #tmp = str(k).split('-')[-1]
                #if str(tmp) not in k: continue
                if f"-{lp}" not in k: continue

                print("LP COUNT " + str(lp) + " key " + str(k))
                
                name = k.replace('tuberculosis-', '').replace('-40-', '-').replace(f'-{lp}', '')
                name = name.replace('0-0', 'el0')
                name = name.replace('1-0', 'el1')
                name = name.replace('1-1', 'el2')
                
                #name = k.replace(f'-{seconds}', '').replace('-40-', '-').replace(f'-{lp}', '')
                #name = name.replace('pcs_hs_lo_re_df', 'el2') 
                #name = name.replace('pcs_hs_lo', 'el1') 
                #name = name.replace('pcs_hs', 'el0') 
                #name = name.replace('pcs_lo_re_df', 'el2') 
                #name = name.replace('pcs_lo', 'el1') 
                #name = name.replace('pcs', 'el0')
                #print("name : " + str(name)) 

            
                if 'seq' in name : 
                    #print(k)
                    baseline = final[k][0] 
                    y += [baseline]
                    #print("BASELINE SEQ: " + str(baseline))
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
                #print("siamo alla fine " + k + " final[k] " + str(final[key]))
                #if str(tmp) not in k: continue
                if f"-{lp}" not in k: continue

                print("LP COUNT 2 " + str(lp) + " key " + str(k))
                
                name = k.replace('tuberculosis-', '').replace('-40-', '-').replace(f'-{lp}', '')
                name = name.replace('0-0', 'el0')
                name = name.replace('1-0', 'el1')
                name = name.replace('1-1', 'el2')

                #name = k.replace(f'-{seconds}', '').replace('-40-', '-').replace(f'-{lp}', '')
                #name = name.replace('pcs_hs_lo_re_df', 'el2') 
                #name = name.replace('pcs_hs_lo', 'el1') 
                #name = name.replace('pcs_hs', 'el0') 
                #name = name.replace('pcs_lo_re_df', 'el2') 
                #name = name.replace('pcs_lo', 'el1') 
                #name = name.replace('pcs', 'el0') 

                print("NAME: " + str(name))
                
                if 'seq' in name: continue

                if name == 'el1':
                    name = 'cache\nopt.'
                elif name == 'el2':
                    name = 'cache\n+ NUMA opt.'
                elif name == 'el0':
                    name = 'baseline'


                print("FINAL NAME : " + name)

                x += [name]
                y += [final[k][0]]
                bp_dict[k]['label'] = name
                cur_bp += [bp_dict[k].copy()]


            for d in cur_bp:
                #print("dictionary: " + str(d))
                for v in d:
                    #print("entries: " + str(v) + " -- " + str(d[v]))
                    if v != 'label':
                        d[v] = d[v] #/baseline


            cur_ax.bxp(cur_bp, showfliers=False, showmeans=True)
            #cur_ax.set_ylim(ran)
            cur_ax.yaxis.grid()
            #cur_ax.yaxis.set_ticks(np.arange(minval, maxval, 0.05))
            #cur_ax.set_ylabel("Speedup w.r.t USE")

        
        plt.savefig(f'figures/{sys.argv[1][:-1].replace("/", "-")}-{test}-9 .pdf')

