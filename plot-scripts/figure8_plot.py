inputFile = "data/figure8.csv"

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
    if "pcs-" in test:
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
    "com_evt" : "Mevents per second",
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





channels=1000.0
tad=120
phb_list=["0.48","0.24","0.12"]
phb_list=["0.4"]
th_list =  [1, 6, 12, 24, 36, 48]
for line in open("thread.conf"):
    if "THREAD_list" in line:
        line = line.split("=")[-1].split("#")[0].replace('"', '').strip().split(' ')
        #print(line)
        th_list = [int(x) for x in line]

th_list =  [48]
ti_list =  [30, 60 ,120, 240]
lp_list=["256", "1024", "4096"]
test="pcs"
q_list = []
for t in th_list:
    q_list += [{"th":str(t)}]


print()
for test in ["pcs", "pcs_hs"]:
    count = -1
    fig, axs = plt.subplots(1,len(lp_list), figsize = (5*len(lp_list),4), sharey=True)
    tit = f"{test} - Platform A"
    fig.suptitle(tit)
    for nlp in lp_list:
        count+=1
        isFirst=True
        for metric in metric_list:
            if len(lp_list) == 1: cur_ax = axs
            else: cur_ax = axs[count] 
            cur_ax.set_xlabel("Seconds")
            cur_ax.set_ylabel(metric_title[metric])
            cur_ax.title.set_text(" ".join(["#cells "+nlp]))
            cur_ax.set_xticks(ti_list)
            #cur_ax.set_yticks(np.arange(0.6,1.25,0.05))
            #plt.gcf().get_axes()[count].set_ylim(0,35)
            metric_idx = columns[metric]
            custom_lines = []
            custom_label = []
            custom_lines = []
            custom_label = []
            for enfl in ["0", "1", "2"]:
                dataplot= [0]
                base= [0]
                x_value= [0]
                for time in ti_list:
                    if enfl == "2":
                        filterDataset  = {k:v[metric_idx]/1000000 for k,v in dataset.items()  if f"{test}_lo_re_df-" in k and str(time) in k and str(nlp) in k}
                        k = f"{test}_lo_re_df-48-{nlp}-{time}"
                    elif enfl == "1":
                        filterDataset  = {k:v[metric_idx]/1000000 for k,v in dataset.items()  if f"{test}_lo-" in k and str(time) in k and str(nlp) in k}
                        k = f"{test}_lo-48-{nlp}-{time}"
                    else:
                        filterDataset  = {k:v[metric_idx]/1000000 for k,v in dataset.items()  if f"{test}-" in k and str(time) in k and str(nlp) in k}
                        k = f"{test}-48-{nlp}-{time}"

                    base_filterDataset  = {k:v[metric_idx]/1000000 for k,v in dataset.items()  if f"{test}-" in k and str(time) in k and str(nlp) in k}
                    k_base = f"{test}-48-{nlp}-{time}"

                    base    += [(base_filterDataset[k_base]-base[-1])/(time-x_value[-1])]
                    dataplot+=[(filterDataset[k]-dataplot[-1])/(time-x_value[-1])]
                    print(enfl, base[-1], dataplot[-1], dataplot[-1]/base[-1])
                    dataplot[-1] = dataplot[-1]/base[-1]
                    x_value += [time]
                cur_ax.plot(x_value[1:], dataplot[1:], color=gran_to_col[0.4], dashes=enfl_to_dash[enfl])
            custom_lines += [Line2D([0], [0], color=gran_to_col[0.4])]
            custom_label += ["ta=0.2"]
            
            for enfl in ["0", "1","2"]:
                custom_lines += [Line2D([0], [0], color='black', dashes=enfl_to_dash[enfl])]
                custom_label += ["el"+str(enfl)]
            
            cur_ax.legend(custom_lines, custom_label,  ncol = 3) #, bbox_to_anchor=(1.12, -0.15))
            
    plt.savefig(f'figures/figure8-{test}.pdf')
