inputFile = "data/figure4.csv"

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

#print(columns)

import matplotlib
matplotlib.use('Agg')
import numpy as np
import matplotlib.pylab as plt

def ratio(a,b):
    return a/b

def inverse_ratio(b,a):
    return a/b

metric_list = ["com_evt", "tot_rol", "evt_gra"]

metric_title={
    "com_evt" : "Overall Speedup",
    "tot_rol" : "Abort probability",
    "evt_gra" : "Event Processing Speedup"
}
metric_function={
    "com_evt" : ratio,
    "tot_rol" : ratio,
    "evt_gra" : inverse_ratio
}
metric_crange={
    "com_evt" : [0.8, 1.35],
    "tot_rol" : [0.6,1.0],
    "evt_gra" : [1.15, 2.5]    
}
ta = "0.24"
lp = "4096"
q_list = [{
"lp":lp,
"ta":ta,
"w" :"0.1"
},
{
"lp":lp,
"ta":ta,
"w" :"0.2"
},
{
"lp":lp,
"ta":ta,
"w" :"0.4"
},
{
"lp":lp,
"ta":ta,
"w" :"0.8"
},
{
"lp":lp,
"ta":ta,
"w" :"1.6"
},
{
"lp":lp,
"ta":ta,
"w" :"3.2"
}
]

ta2rho={
    "0.12":r"$\rho$"+":1.00",
    "0.16":r"$\rho$"+":0.75",
    "0.24":r"$\rho$"+":0.50",
    "0.48":r"$\rho$"+":0.25",
}



for line in open("thread.conf"):
    if "MAX_THREADS" in line:
        line = line.split("=")[-1].split("#")[0].replace('"', '').strip()
        th = int(line)


fig, axs = plt.subplots(1,len(metric_list), figsize = (3*len(metric_list),3))
tit = " ".join(["#simulation objects:4096"])
tit = tit.replace("ta:0.08", r"$\rho$"+":0.50")
fig.suptitle(tit)
fig.tight_layout(pad=3.0)
count = -1
for metric in metric_list:
    count+=1
    axs[count].set_xlabel("Windows Size W (s)")
    axs[count].set_ylabel(metric_title[metric])
    #plt.gcf().get_axes()[count].set_ylim(metric_crange[metric][0],metric_crange[metric][1])
    for ta in ["0.48", "0.24", "0.16", "0.12"]:
        dataplot= []
        x_value= []
        for query in q_list:
            #axs[count].set_title(metric_title[metric]) 
            query["ta"]=ta
            base_query=query.copy()
            base_query["w"]="0.1"

            metric_idx = columns[metric]
            baseline_value = [v[metric_idx] for k,v in baseline.items() if checkTest(k, base_query)][0]
            filtereDataset  = {k:v[metric_idx] for k,v in dataset.items()  if checkTest(k, query)}
            
            x_value += [float(query["w"])]
            i = 2
            j = 2
            k = "pcs_el_1-w_"+query["w"]+"-cbs_"+str(i)+"-ebs_"+str(j)+"-ta_"+query["ta"]+"-tad_120-tac_300_th_"+str(th)+"-lp_"+query["lp"]
            
            if metric == "tot_rol":
                bas_tot = [v[columns["tot_evt"]] for k,v in baseline.items() if checkTest(k, base_query)][0]
                bas_sil = [v[columns["sil_evt"]] for k,v in baseline.items() if checkTest(k, base_query)][0]
                bas_rol = [v[columns["tot_rol"]] for k,v in baseline.items() if checkTest(k, base_query)][0]
                baseline_value = baseline_value/(bas_tot-bas_sil)
                dat_tot = {k:v[columns["tot_evt"]] for k,v in dataset.items()  if checkTest(k, query)}[k]
                dat_sil = {k:v[columns["sil_evt"]] for k,v in dataset.items()  if checkTest(k, query)}[k]
                dat_rol = {k:v[columns["tot_rol"]] for k,v in dataset.items()  if checkTest(k, query)}[k]
                dat_str = {k:v[columns["str_evt"]] for k,v in dataset.items()  if checkTest(k, query)}[k]
                dat_ant = {k:v[columns["ant_evt"]] for k,v in dataset.items()  if checkTest(k, query)}[k]
                dat_com = {k:v[columns["com_evt"]] for k,v in dataset.items()  if checkTest(k, query)}[k]
                dat_abo = {k:v[columns["abo_evt"]] for k,v in dataset.items()  if checkTest(k, query)}[k]
                baseline_value = (dat_tot-dat_sil)/100
                dataplot+=[ (dat_abo)/(dat_tot-dat_sil)  ]
                
            else:
                    dataplot+=[metric_function[metric](filtereDataset[k],baseline_value)]
        axs[count].plot(x_value, dataplot, label=ta2rho[ta])
    if metric != "tot_rol":
        axs[count].legend(loc='lower right')
    else:
        axs[count].legend(loc='upper left')
        
plt.savefig('figures/figure4.pdf')