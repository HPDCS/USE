#!/bin/python3

fn = 'data/table4.csv'

f = open(fn)
cnt = 0

res = []



max_treads='40'

for line in open("thread.conf"):
    if "MAX_THREADS" in line:
        line = line.split("=")[-1].split("#")[0].replace('"', '').strip().split(' ')
        #print(line)
        max_treads = str(max([int(x) for x in line]))

for line in f.readlines():
  if 'threads_1-' in line: continue
  if 'seq' in line: continue
  if 'test' in line: continue
  line = line.strip().split(',')[:7]
  
  tot = float(line[1]) - float(line[-1])
  rol = float(line[3])+float(line[4])
  pro = rol*100.0/tot

  res += [(line[0],pro)]

  cnt +=1
f.close()

res=sorted(res)

sim=["enfl_0-numa_0", "enfl_1-numa_0", "enfl_1-numa_1"]

d={}
for s in sim:
  d[s] = 0

for t,p in res:
  s = "-".join(t.split("-")[:2])
  d[s] = p

for s in sim:
  line=s #.replace("_", "\\_")
  line += " "+f'& {"{:.4f}".format(  d[s])}\\%  '
  line+=" \\\\ \\hline"
  line = line.replace("enfl_1-numa_1", '{\\bf cache+NUMA opt.}').replace("enfl_1-numa_0", '{\\bf cache opt.}').replace("enfl_0-numa_0",'{\\bf baseline}')
  print(line)
