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
    #return a/b
    return (a/20.0)/1000000.0

def inverse_ratio(b,a):
    return a/b

metric_list = ["com_evt"]

metric_title={
    "com_evt" : "Throughput (Mevts per sec)",
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
    q_list += [{"lp":"1024","threads":str(t)}]

fig, axs = plt.subplots(1,len(metric_list), figsize = (5*len(metric_list),3))
tit = " ".join(["#simulation objects:1024"])
fig.suptitle(tit)

count = -1
for metric in metric_list:
    count+=1
    if len(metric_list) == 1: cur_ax = axs
    else: cur_ax = axs[count] 
    cur_ax.set_xlabel("Granularity ("+r"$\mu$"+"s)")
    cur_ax.set_ylabel(metric_title[metric])
    metric_idx = columns[metric]
    custom_lines = []
    custom_label = []
    isFirst = True
    newdata_x = [[], []]
    newdata_y = [[], []]
    dataset_per_th = {}
    for g in gran_list:
        g = str(g)
        for enfl in ["0", "1"]:
            dataplot= []
            x_value= []
            for query in q_list:
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
                k = "phold-enfl_"+enfl+"-threads_"+query["threads"]+"-lp_"+query["lp"]+"-maxlp_1000000-look_0-fan_1-loop_"+query["loop"]
                dataplot+=[metric_function[metric](filtereDataset[k],baseline_value)] #baseline_value[k.replace("enfl_1", "enfl_0")])]   
            #print(g,dataplot)
            newdata_x[int(enfl)] += [int(g)]
            newdata_y[int(enfl)] += [[dataplot[0],dataplot[1],dataplot[3],dataplot[-1]]]
            #newdata_y[int(enfl)] += [dataplot]
            if isFirst and False:
                custom_lines += [Line2D([0], [0], color='black', dashes=enfl_to_dash[enfl])]
                if enfl == '0':
                    custom_label += ["USE"]
                if enfl == '1':
                    custom_label += ["cache+NUMA opt."]
            
        if isFirst: 
            isFirst = False
    #print(newdata_x, newdata_y) 
    #print(newdata_x[0], newdata_y[0]) 

    final_y = []

    for i in range(len(newdata_y[0])):
        #for j in range(len(newdata_y[0][i])):
        #  pass
        #print(newdata_x[0][i], newdata_y[0][i][1]/newdata_y[0][i][0])
        pass #print()
        
    #print(final_y)
    cnt=0
    #print(newdata_y[0])
    cur_ax.plot(newdata_x[0], [v[0] for v in newdata_y[0]], color=gran_to_col['1'], dashes=enfl_to_dash[str(0)], label="USE")
    #cur_ax.plot(newdata_x[1], [v[0] for v in newdata_y[1]], color=gran_to_col['1'], dashes=enfl_to_dash[str(1)], label="USE +opt")
    cur_ax.plot(newdata_x[0], [v[1] for v in newdata_y[0]], color=gran_to_col['5'], dashes=enfl_to_dash[str(0)], label="USE")
    cur_ax.plot(newdata_x[1], [v[1] for v in newdata_y[1]], color=gran_to_col['5'], dashes=enfl_to_dash[str(1)], label="USE +opt")
    cur_ax.plot(newdata_x[0], [v[2] for v in newdata_y[0]], color=gran_to_col['25'], dashes=enfl_to_dash[str(0)], label="USE")
    cur_ax.plot(newdata_x[1], [v[2] for v in newdata_y[1]], color=gran_to_col['25'], dashes=enfl_to_dash[str(1)], label="USE +opt")
    cur_ax.plot(newdata_x[0], [v[3] for v in newdata_y[0]], color=gran_to_col['50'], dashes=enfl_to_dash[str(0)], label="USE")
    cur_ax.plot(newdata_x[1], [v[3] for v in newdata_y[1]], color=gran_to_col['50'], dashes=enfl_to_dash[str(1)], label="USE +opt")
                            
    #print(newdata_x[0][-1],newdata_y[1][-1][-1])
    #cur_ax.plot(newdata_x[0][-1],newdata_y[1][-1][-1], marker='o', color='r') 
    #cur_ax.plot(newdata_x[0][6],newdata_y[1][6][-1], marker='o', color='green') 
    #cur_ax.plot(newdata_x[0][3],newdata_y[1][3][-1], marker='o', color='blue') 
    #cur_ax.plot(newdata_x[0][4],newdata_y[1][4][-1], marker='o', color='purple') 


    for g in [3,5,6,7]:
        for t in [-1, -2, -3]:
            cur_ax.annotate("x{:.1f}".format(newdata_y[1][g][t]/newdata_y[0][g][0]),(newdata_x[0][g],newdata_y[1][g][t]), size=8) 
    
    custom_lines += [Line2D([0], [0], color='black', dashes=enfl_to_dash[str(0)])]
    custom_lines += [Line2D([0], [0], color='black', dashes=enfl_to_dash[str(1)])]
    custom_label += ["USE"]
    custom_label += ["OPT"]
    custom_lines += [Line2D([0], [0], color=gran_to_col['1'], dashes=enfl_to_dash[str(0)])]
    custom_label += ["1 threads"]
    custom_lines += [Line2D([0], [0], color=gran_to_col['5'], dashes=enfl_to_dash[str(0)])]
    custom_label += ["5 threads"]
    custom_lines += [Line2D([0], [0], color=gran_to_col['25'], dashes=enfl_to_dash[str(0)])]
    custom_label += ["20 threads"]
    custom_lines += [Line2D([0], [0], color=gran_to_col['50'], dashes=enfl_to_dash[str(0)])]
    custom_label += ["40 threads"]
    #cur_ax.legend(custom_lines, custom_label,loc="upper right",  ncol = 1, bbox_to_anchor=(1.4, 1.05))
    cur_ax.legend(custom_lines, custom_label) #,loc="upper right",  ncol = 1, bbox_to_anchor=(1.4, 1.05))
    cur_ax.set_xlim(-10,440)
    
plt.savefig('figures_reproduced/figure8.pdf', bbox_inches='tight')
cur_ax.set_yscale("log", base=2)

plt.savefig('figures_reproduced/figure8-log.pdf', bbox_inches='tight')
