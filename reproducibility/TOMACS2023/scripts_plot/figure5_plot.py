inputFile = "data/figure5.csv"

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
    if "el_0" in test:
        baseline[test] = [float(x) for x in line.split(",")[1:]]
    else:
        dataset[test] = [float(x) for x in line.split(",")[1:]]

count = 0
for k in header.split(",")[1:]:
    columns[k] = count
    count+=1

import matplotlib
matplotlib.use('Agg')
import numpy as np
import seaborn as sns
import matplotlib.pylab as plt
from matplotlib.colors import LinearSegmentedColormap
cmap = sns.cm.rocket_r
cmap_name = 'my_list'

def ratio(a,b):
    return a/b

def inverse_ratio(b,a):
    return a/b

metric_list = ["com_evt", "tot_rol", "evt_gra"]
metric_title={
    "com_evt" : "Overall Speedup",
    "tot_rol" : "Abort probability ratio",
    "evt_gra" : "Event Processing Speedup"
}
metric_function={
    "com_evt" : ratio,
    "tot_rol" : ratio,
    "evt_gra" : inverse_ratio
}
ta = "0.24"
lp = "4096"
q_list = [
#{
#"lp":lp,
#"ta":ta,
#"w" :"0.1"
#},
#{
#"lp":lp,
#"ta":ta,
#"w" :"0.2"
#},
#{
#"lp":lp,
#"ta":ta,
#"w" :"0.4"
#},
#{
#"lp":lp,
#"ta":ta,
#"w" :"0.8"
#},
#{
#"lp":lp,
#"ta":ta,
#"w" :"1.6"
#},
{
"lp":lp,
"ta":ta,
"w" :"3.2"
}
]


for line in open("thread.conf"):
    if "MAX_THREADS" in line:
        line = line.split("=")[-1].split("#")[0].replace('"', '').strip()
        th = int(line)

for query in q_list:
    fig, axs = plt.subplots(1,len(metric_list), figsize = (3*len(metric_list),2), sharey=True)
    tit = " ".join([k+":"+v for k,v in query.items()])
    tit = tit.replace("ta:0.08", r"$\rho$"+":0.50")
    tit = tit.replace("ta:0.24", r"$\rho$"+":0.50")
    tit = tit.replace("lp",  "#simulation object")
    tit = tit.replace("w",  "W")
    fig.suptitle(tit,y=1.12)


    count = 0
    for metric in metric_list:
        axs[count].set_title(metric_title[metric]) 
        base_query=query.copy()
        base_query["w"]="3.2"

        metric_idx = columns[metric]
        #print([v[metric_idx] for k,v in baseline.items() if checkTest(k, base_query)])
        baseline_value = [v[metric_idx] for k,v in baseline.items() if checkTest(k, base_query)][0]
        filtereDataset  = {k:v[metric_idx] for k,v in dataset.items()  if checkTest(k, query)}
        
        ebs_list = [1,2,4,8]
        cbs_list = ebs_list[::-1]

        dataplot= []

        for i in cbs_list:
            row = []
            for j in ebs_list:
                k = "pcs_el_1-w_"+query["w"]+"-cbs_"+str(i)+"-ebs_"+str(j)+"-ta_"+query["ta"]+"-tad_120-tac_300_th_"+str(th)+"-lp_"+query["lp"]
                #row += [metric_function[metric](filtereDataset[k],baseline_value)]
                
                if metric == "tot_rol":
                    bas_tot = [v[columns["tot_evt"]] for k,v in baseline.items() if checkTest(k, base_query)][0]
                    bas_sil = [v[columns["sil_evt"]] for k,v in baseline.items() if checkTest(k, base_query)][0]
                    bas_abo = [v[columns["abo_evt"]] for k,v in baseline.items() if checkTest(k, base_query)][0]
                    baseline_value = bas_abo/(bas_tot-bas_sil)
                    dat_tot = {k:v[columns["tot_evt"]] for k,v in dataset.items()  if checkTest(k, query)}[k]
                    dat_sil = {k:v[columns["sil_evt"]] for k,v in dataset.items()  if checkTest(k, query)}[k]
                    dat_abo = {k:v[columns["abo_evt"]] for k,v in dataset.items()  if checkTest(k, query)}[k]
                    dat_value = (dat_abo)/float(dat_tot-dat_sil)
                    row+=[ dat_value/baseline_value ]
                else:
                    row+=[metric_function[metric](filtereDataset[k],baseline_value)]    

            
            dataplot+=[row]

        colors = [(0,(1, 1.0, 1)), (1,(0.2, 0.45, 0.25))]  # R -> G -> B
        cmap = LinearSegmentedColormap.from_list(cmap_name, colors, N=1000)
            
        if metric != "tot_rol":
            sns.heatmap(dataplot, ax=axs[count], square=True, linewidth=0.5, \
                    xticklabels=ebs_list, yticklabels=cbs_list, annot=True, fmt=".2f", cmap=cmap, vmin=1.0) 
        else:
            sns.heatmap(dataplot, ax=axs[count], square=True, linewidth=0.5, \
                    xticklabels=ebs_list, yticklabels=cbs_list, annot=True, fmt=".0f", cmap=cmap)
            

        axs[count].set_xlabel("Evicted pipe size")
        axs[count].set_ylabel("Pipe size")
        count+=1
    plt.savefig("figures_reproduced/figure5-"+"-".join([k+"_"+v for k,v in query.items()])+'.pdf', bbox_inches='tight')