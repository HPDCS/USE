#!/bin/python3

files = ['8a', '8b', '8c']
sims = ['pcs', 'pcs_lo', 'pcs_lo_re_df']
lps = set([])


dataset = {}
mins = {}


for f in files:
  fn = 'data/figure'+f+'.csv'
  fn = open(fn)
  count=0
  for line in fn.readlines():
    line = line.strip().split(',')
    if count == 0:
      count+=1
      print(line[1], line[3], line[4], line[5])
      continue
    #line=line.strip().split(',')
    if 'seq' in line[0]: continue
    if f not in dataset: dataset[f] = {}
    test = line[0].split('-')
    #print(test)
    lp = int(test[2])
    lps.add(lp)
    if lp not in dataset[f]: dataset[f][lp] = {}
    dataset[f][lp][test[0]] = ( 100.0*float(line[3])+float(line[4]) ) / ( float(line[1])-float(line[5]) )



line = []
for k1 in files:
  for k2 in sorted(lps):
    line += [str(k2)]
k = '\multicolumn{2}{l||}{\#SOs} & '
res = k+' '*(12-len(k))+' & '.join(line)
res += ' \\\\ \\hline \\hline'
print(res)

for k in sims:
  line = []
  for k1 in files:
    for k2 in sorted(lps):
      #print(k,k1,k2)
      res  = ""
      #res += k+' '+ k1+' '+str(k2)+' '
      line += [ res + str("{:10.2f}".format( dataset[k1][k2][k] ))+'\%' ]
  res = ""
  if k == 'pcs': res += '\multirow{3}{*}{\\sc Version } & '
  else: res += '{} & '
  res += '{' + k + '}' +' '*(16-len(k))+'& '+' & '.join(line)
  if k != 'pcs_lo_re_df': res += ' \\\\ \\hline'
  res = res.replace('pcs_lo_re_df', 'cache+NUMA opt.').replace('pcs_lo', 'cache opt.').replace('pcs','baseline')
  print(res)
