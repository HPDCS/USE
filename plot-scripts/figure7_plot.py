inputFile = "data/figure7.csv"

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
    if "enfl_0-" in test:
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

gran_list=[1, 5, 10, 25, 50, 100, 200, 400]
gran_to_col={
    "1"  :"darkcyan",
    "5"  :"goldenrod",
    "10" :"silver",
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

threads_list =  [1, 6, 12, 24, 36, 48]
for line in open("thread.conf"):
    if "THREAD_list" in line:
        line = line.split("=")[-1].split("#")[0].replace('"', '').strip().split(' ')
        #print(line)
        threads_list = [int(x) for x in line]

q_list = []
for t in threads_list:
    if t == "1":
    	continue;
    q_list += [{"lp":"1024","threads":str(t)}]

fig, axs = plt.subplots(1,len(metric_list), figsize = (5*len(metric_list),3))
tit = " ".join(["#simulation objects:1024"])
fig.suptitle(tit)

count = -1
for metric in metric_list:
    count+=1
    if len(metric_list) == 1: cur_ax = axs
    else: cur_ax = axs[count] 
    cur_ax.set_xlabel("#Threads")
    cur_ax.set_ylabel(metric_title[metric])
    metric_idx = columns[metric]
    custom_lines = []
    custom_label = []
    isFirst = True
    for g in gran_list:
        g = str(g)
        for enfl in ["0", "1"]:
            dataplot= []
            x_value= []
            for query in q_list:
                if query["threads"] == "1":
                	continue;
                #print(query)
                query["enfl"]=enfl
                query["loop"]=g
                base_query=query.copy()
                base_query["enfl"]="0"
                base_query["threads"]="1"
                filtereDataset  = {k:v[metric_idx] for k,v in dataset.items()  if checkTest(k, query)}
                dir = {k:v[metric_idx] for k,v in baseline.items() if checkTest(k, base_query)}
                baseline_value  = [v[metric_idx] for k,v in baseline.items() if checkTest(k, base_query)][0]
                x_value += [float(query["threads"])]
                i = 2
                j = 2
                k = "phold-enfl_"+enfl+"-threads_"+query["threads"]+"-lp_"+query["lp"]+"-maxlp_1000000-look_0-ck_per_20-fan_1-loop_"+query["loop"]
                dataplot+=[metric_function[metric](filtereDataset[k],baseline_value)] #baseline_value[k.replace("enfl_1", "enfl_0")])]    
                cur_ax.plot(x_value, dataplot, color=gran_to_col[g], dashes=enfl_to_dash[enfl])
            if isFirst:
                custom_lines += [Line2D([0], [0], color='black', dashes=enfl_to_dash[enfl])]
                if enfl == '0':
                    custom_label += ["USE"]
                if enfl == '1':
                    custom_label += ["cache+NUMA opt."]
            
        if isFirst: 
            isFirst = False
            
        custom_lines += [Line2D([0], [0], color=gran_to_col[g])]
        custom_label += [str(g)+r"$\mu$"+"s"]
    cur_ax.legend(custom_lines, custom_label,loc="upper right",  ncol = 1, bbox_to_anchor=(1.6, 1.05))

    
plt.savefig('figures/figure7.pdf', bbox_inches='tight')
cur_ax.set_yscale("log", base=2)

plt.savefig('figures/figure7-log.pdf', bbox_inches='tight')
