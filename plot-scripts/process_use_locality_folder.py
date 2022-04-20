import sys
from os import listdir
from os.path import isfile, join

mypath =sys.argv[1]
onlyfiles = [f for f in listdir(mypath) if isfile(join(mypath, f))]


fields_grep = {
"Total event"   : "tot_evt",
"Committed eve" : "com_evt",
"Straggler"     : "str_evt",
"Anti events"   : "ant_evt",
"Useless even"  : "abo_evt",
"Silent events.": "sil_evt",
"Avg node"      : "avg_nod",
"Total Clock"   : "tot_clo",
"Event Proc"    : "evt_clo",
"time to proce" : "evt_gra",
"local ind"     : "loc_idx",
"global ind"    : "glo_idx",
"Rollback opera": "tot_rol",
#"FILENAMENOTEX" : "fname",
}


columns = fields_grep.values()
data = {}

#print(columns)
print("test,"+",".join(columns))
for fname in onlyfiles:
  fname = mypath+fname
  entry = {}
  for c in columns: entry[c] = ""
  entry["fname"] = fname 

  for line in open(fname).readlines():
    line = line.strip()
    for k in fields_grep:
      if k in line:
        col = fields_grep[k]
        value = line.split(":")[1].strip()
        if " " in value: value = value.split(" ")[0].strip()
        entry[col] = float(value)
  
  res_line = []
  for c in columns:
    res_line += [entry[c]]
  key = fname.split("-")[:-1]
  key = "-".join(key)
  key = key.split("/")[-1]
  #print(key)
  if key not in data:
    data[key] = []
  data[key] += [res_line]

#for k in data:
#  print(k,data[k])


avg = {}

for k in data:
  li = data[k]
  #print(k,data[k])
  nli = [0] * len(li[0])
  samples=len(li)
  for l in li:
    try:
      for i in range(len(l)):
        nli[i] += l[i]
    except:
      samples-=1
      continue
  for i in range(len(nli)):
    nli[i] = nli[i]/samples
  avg[k] = nli
  print(k+","+",".join([str(x) for x in avg[k]]))
