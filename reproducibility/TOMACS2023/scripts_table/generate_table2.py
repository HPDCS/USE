#!/bin/python3

fn = 'data/table2.csv'

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


uti=["0.48", "0.24", "0.12"]
lps=["256", "1024", "4096"]
sim=["pcs", "pcs_lo", "pcs_lo_re_df"]

d={}
for s in sim:
  d[s] = {}
  for u in uti:
      d[s][u] = {}
      for l in lps:
        d[s][u][l] = 0

for t,p in res:
  u = t.split("-")[0]
  s = "-".join(t.split("-")[1:]).split(f"-{max_treads}-")[0]
  l = t.split("-")[-2]
  d[s][u][l] = p
  #print(s,u,l,p)



for s in sim:
  line=s #.replace("_", "\\_")
  for u in uti:
      for l in lps:
        line += " "+f'& {"{:.2f}".format(  d[s][u][l])}\\%  '
        #print(s,u,l,f'& {"{:.4f}".format(p)}\\%  \\\\')
  line+=" \\\\ \\hline"
  line = line.replace('pcs_lo_re_df', '{\\bf cache+NUMA opt.}').replace('pcs_lo', '{\\bf cache opt.}').replace('pcs','{\\bf baseline}')
  print(line)



#print(res)
