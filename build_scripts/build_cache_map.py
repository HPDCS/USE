import sys

d = {}
rd = {}
count=0
for line in open(sys.argv[1]).readlines():
  count+=1
  if count == 1: continue
  if "Instruction" in line: continue
  line = line.strip()
  line = line.split(' ')
  cache_id = (line[0],line[2])
  if cache_id not in d:
     d[cache_id] = []
  d[cache_id] += [line[1]]

for k in d:
 nk = frozenset(d[k])
 if nk not in rd:
   rd[nk] = []
 rd[nk] += [k]
 #print(k,d[k])

#print("")

for k in rd:
  nv = []
  for v in rd[k]:
    nv += [(int(v[0]), int(v[1].replace('index', '')))]
  rd[k] = sorted(nv, key=lambda x:x[1])[-1]
  #print(k,rd[k])

#print("")

res={}
check={}

for k in rd:
  l = list(k)
  for i in range(len(l)-1):
    for j in range(len(l))[i+1:]:
      a = int(l[i].replace('cpu', ''))
      b = int(l[j].replace('cpu', ''))
      if a not in res:
        res[a] = []
        check[a] = set([])
      if b not in res:
        res[b] = []
        check[b] = set([])
      if b not in check[a]:
        res[a] += [(b,rd[k][1])]
        #check[a].add(b)
      if a not in check[b]:
        res[b] += [(a,rd[k][1])]
        #check[b].add(a)


for k in res:
  res[k] = sorted(res[k], key=lambda x:x[1])
  #print(str(k)+":"+str(len(res[k]))+','+str(res[k]).replace('[', '').replace(']', ''))

#print("")


fin = {}
for k in res:
  l = res[k]
  check = set([])
  if k not in fin: fin[k] = []
  for i in range(len(l)):
      a,va = l[i]
      if a not in check:
        fin[k] += [a]
        check.add(a)
  len2 = len(fin[k])

len1 = len(fin)
print("#define HPIPE_INDEX2_LEN "+str(len2+1))
print("#define HPIPE_INDEX1_LEN "+str(len1)+"\n")

for k in fin:
  print("int hpipe_index2_cpu"+str(k)+'[HPIPE_INDEX2_LEN]='+str([k]+fin[k]).replace('[', '{').replace(']', '}')+';')

print("")

print('int *hpipe_index1[HPIPE_INDEX1_LEN]={')
for k in range(len1)[:-1]:
  print("hpipe_index2_cpu"+str(k)+',')

print("hpipe_index2_cpu"+str(len1-1)+"\n};")



