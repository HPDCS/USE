import json

f = "doc-coverage.info"
f = open(f)
f = json.load(f)

res = {}
fin = {}

for k in f:
  res[k] = {}
  #print(k)
  for field in f[k]:
    #print(field)
    kind = field["kind"]
    if kind not in res[k]:
      res[k][kind] = [0,0]
      fin[kind] = [0,0]
    res[k][kind][1] += 1
    if field["documented"]:
     res[k][kind][0] += 1


for k in res:
  #print(k,res[k])
  for t in res[k]:
    #print(t, res[k][t])
    fin[t][0] += res[k][t][0]
    fin[t][1] += res[k][t][1]


l = 12
fields = ["KIND","DOCUMENTED","TOTAL","PERCENTAGE"]
fields = [' '*(l-len(f))+f for f in fields]
print(" | ".join(fields))
fields = ["-"*12]*4
print(" | ".join(fields))


res = [0,0]
for k in fin:
  fields = [k,str(fin[k][0]),str(fin[k][1]),"{:.2f}".format(float(fin[k][0])*100.0/fin[k][1])+' %']
  fields = [' '*(l-len(f))+f for f in fields]
  print(" | ".join(fields))
  res[0] += fin[k][0]
  res[1] += fin[k][1]

fields = ["-"*12]*4
print(" | ".join(fields))

fields = ["total",str(res[0]),str(res[1]),"{:.2f}".format(float(res[0])*100.0/res[1])+' %']

fields = [' '*(l-len(f))+f for f in fields]
print(" | ".join(fields))
