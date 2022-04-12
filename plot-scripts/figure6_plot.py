inputFile = "data/figure6.csv"

dataset={}
baseline={}
columns={}


def checkTest(test, q):
    for k in q:
        for pair in test.split("-"):
            if k==pair.split("_")[0] and k+"_"+q[k] != pair:
                return False
    return True

def filterData(l, idxs):
    return [ l[i] for i in idxs]


f = open(inputFile)
header=""
count=0
for line in f:
    line = line.strip()
    if count == 0:
        header = line
        count+=1
        continue
    test=line.split(",")[0]
    if "el_0-" in test:
        baseline[test] = [float(x) for x in line.split(",")[1:]]
        dataset[test] = [float(x) for x in line.split(",")[1:]]
    else:
        dataset[test] = [float(x) for x in line.split(",")[1:]]

count = 0
for k in header.split(",")[1:]:
    columns[k] = count
    count+=1

#print(columns)

import matplotlib
matplotlib.use('Agg')
import numpy as np
import matplotlib.pylab as plt
from matplotlib.lines import Line2D

def ratio(a,b):
    return a/b

def inverse_ratio(b,a):
    return a/b

metric_list = ["com_evt"]

metric_title={
    "com_evt" : "Overall Speedup",
    "avg_nod" : "Overall Speedup",
    "tot_rol" : "Abort probability",
    "evt_gra" : "Event Processing Speedup"
}
metric_function={
    "com_evt" : ratio,
    "tot_evt" : ratio,
    "avg_nod" : ratio,
    "evt_gra" : inverse_ratio
}

gran_to_col={
    0.25  :"darkcyan",
    0.50  :"goldenrod",
    1.0   :"indigo",
    "25" :"chocolate",
    "50" :"darkblue",
    "100":"indigo",
    "200":"darkgreen",
    "400":"darkred",
}

enfl_to_dash={
    "0":[1, 0],
    "1":[1, 1],
}





channels=1000.0
tad=120
phb_list=["0.48","0.24","0.12"]
th_list =  [1, 6, 12, 24, 36, 48]
for line in open("thread.conf"):
    if "THREAD_list" in line:
        line = line.split("=")[-1].split("#")[0].replace('"', '').strip().split(' ')
        #print(line)
        threads_list = [int(x) for x in line]

lp_list=["256", "1024", "4096"]
test="pcs"
q_list = []
for t in th_list:
    q_list += [{"th":str(t)}]


fig, axs = plt.subplots(1,len(phb_list), figsize = (5*len(phb_list),4), sharey=True)
tit = "PCS - Platform A"
fig.suptitle(tit)

count = -1
for nlp in lp_list:
    count+=1
    isFirst=True
    for metric in metric_list:
        if len(phb_list) == 1: cur_ax = axs
        else: cur_ax = axs[count] 
        cur_ax.set_xlabel("#Threads")
        cur_ax.set_ylabel(metric_title[metric])
        cur_ax.title.set_text(" ".join(["#cells "+nlp]))
        cur_ax.set_xticks(th_list)
        cur_ax.set_yticks(np.arange(0.6,1.25,0.05))
        #plt.gcf().get_axes()[count].set_ylim(0,35)
        metric_idx = columns[metric]
        custom_lines = []
        custom_label = []
        for phs in phb_list:
            rho = (float(tad)/float(phs))/channels
            for enfl in ["0","1"]:
                dataplot= []
                x_value= []
                for query in q_list:
                    query["el"]=enfl
                    query["lp"]=nlp
                    query["ta"]=phs
                    base_query=query.copy()
                    base_query["el"]="0"
                    #base_query["th"]="1"
                    filtereDataset  = {k:v[metric_idx] for k,v in dataset.items()  if checkTest(k, query)}
                    base  = {k:v[metric_idx] for k,v in baseline.items() if checkTest(k, base_query)}
                    baseline_value  = [v[metric_idx] for k,v in baseline.items() if checkTest(k, base_query)][0]
                    x_value += [float(query["th"])]
                    k = test+"_el_"+enfl+"-nch_1000-ta_"+query["ta"]+"-tad_"+str(tad)+"-tac_300-th_"+query["th"]+"-lp_"+query["lp"]
                    dataplot+=[metric_function[metric](filtereDataset[k],baseline_value)]
                    cur_ax.plot(x_value, dataplot, color=gran_to_col[rho], dashes=enfl_to_dash[enfl])
            custom_lines += [Line2D([0], [0], color=gran_to_col[rho])]
            custom_label += [r"$\rho$="+str(rho)]
        
        for enfl in ["0","1"]:
            custom_lines += [Line2D([0], [0], color='black', dashes=enfl_to_dash[enfl])]
            custom_label += ["el"+str(enfl)]
        
        cur_ax.legend(custom_lines, custom_label,  ncol = 2) #, bbox_to_anchor=(1.12, -0.15))
        
plt.savefig('figures/figure6.pdf')