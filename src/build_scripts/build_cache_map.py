import sys

cpus_per_cache = {}
caches_per_cpu = {}
count=0
cpus=[]
cpu_set=set([])
n_cpus=0

for line in open(sys.argv[1]).readlines():
  count+=1
  if count == 1: continue
  if "Instruction" in line: continue
  line = line.strip()
  line = line.split(' ')
  cache_id = (line[0],line[2])
  if line[1] not in cpu_set:
    cpu_set.add(line[1])
    cpus += [line[1]]
  if cache_id not in cpus_per_cache:  cpus_per_cache[cache_id] = []
  cpus_per_cache[cache_id] += [line[1]]

#print("CPUS per CACHE",cpus_per_cache)
#print("CPUS",cpus)
n_cpus = len(set(cpus))

for k in cpus_per_cache:
 nk = frozenset(cpus_per_cache[k])
 if nk not in caches_per_cpu: caches_per_cpu[nk] = []
 caches_per_cpu[nk] += [k]

#print("N_CPU",n_cpus)
#print("CACHE per CPUs", caches_per_cpu)

for k in caches_per_cpu:
  nv = []
  for v in caches_per_cpu[k]:
    nv += [(int(v[0]), int(v[1].replace('index', '')))]
  #print(nv)
  caches_per_cpu[k] = sorted(nv, key=lambda x:x[1])[-1]

#print("LAST_CACHE per CPUs", caches_per_cpu)

res={}
check={}
#len2=0

for cpu in cpus:
  a = int(cpu.replace('cpu', ''))
  if a not in res: 
    res[a] = []
    check[a] = set([])


for k in caches_per_cpu:
  l = list(k)
  #print(l)
  for i in range(len(l)-1):
    a = int(l[i].replace('cpu', ''))
    #if a not in res: 
    #  res[a] = []
    #  check[a] = set([])
    for j in range(len(l))[i+1:]:
      #a = int(l[i].replace('cpu', ''))
      b = int(l[j].replace('cpu', ''))
      #if a not in res:
      #  res[a] = []
      #  check[a] = set([])
      if b not in res:
        res[b] = []
        check[b] = set([])
      if b not in check[a]:
        res[a] += [(b,caches_per_cpu[k][1])]
        #check[a].add(b)
      if a not in check[b]:
        res[b] += [(a,caches_per_cpu[k][1])]
        #check[b].add(a)

#print("RES",res)

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
