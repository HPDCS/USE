import sys

files=[
"plots_phold/TH-phold-lf-dymelor-hijacker-60-maxlp-1000000-look-0-ck_per-50-fan-1-loop-500",
"plots_phold/TH-phold-lf-dymelor-hijacker-256-maxlp-1000000-look-0-ck_per-50-fan-1-loop-500",
"plots_phold/TH-phold-lf-dymelor-hijacker-1024-maxlp-1000000-look-0-ck_per-50-fan-1-loop-500",
"plots_traffic/TH-traffic_137_03_08",
"plots_pholdhotspotmixed_th_correct/TH-pholdhotspotmixed-lf-dymelor-hijacker-nhs2-psf0-w999-h8-pop200-pws10-1024-maxlp-1000000-look-0-ck_per-50-fan-5-loop-500",
"plots_pholdhotspotmixed_th_correct/TH-pholdhotspotmixed-lf-dymelor-hijacker-nhs5-psf0-w999-h8-pop200-pws10-1024-maxlp-1000000-look-0-ck_per-50-fan-5-loop-500",
]

files_names=[
"phold-60",
"phold-256",
"phold-1024",
"traffic",
"phold-hs-mixed-2",
"phold-hs-mixed-5",
]

powercap = float(sys.argv[1])


fheader = ["strategy"]
fline = ["static"]
fres = []
for f in files_names:
	fheader += [f]

fres += [fheader]

for fth in files:
	fpo = fth.replace("TH", "PO")

	lth = []
	lpo = []
	header = ""
	count = 0
	res = []

	for line in open(fth).readlines():
		if count == 0:
			header = [line.strip().split()]
		else:
			lth += [line.strip().split()]
		count+=1

	count = 0
	for line in open(fpo).readlines():
		if count == 0:
			header = [line.strip().split()]
		else:
			lpo += [line.strip().split()]
		count+=1

	for i in range(len(lth)):
		for j in range(len(lth[i]))[1:]:
			res += [(lth[i][0], header[0][j], float(lth[i][j]), float(lpo[i][j]))]


	res.sort(key=lambda x: x[2] if x[3] <= powercap else 0, reverse=True)
	sys.stderr.write(\
        fth.replace("plots_traffic/TH-traffic_137_03_08", "traffic").replace("plots_phold/TH-phold-lf-dymelor-hijacker", "phold")
        .replace("-maxlp-1000000-look-0-ck_per-50-fan-1-loop-500", "")
        .replace("-maxlp-1000000-look-0-ck_per-50-fan-5-loop-500", "")
        .replace("plots_pholdhotspotmixed_th_correct/TH-pholdhotspotmixed-lf-dymelor-hijacker", "pholdhotspotmixed")	
        .replace("-psf0-w999-h8-pop200-pws10-1024", "")	
		+"\t"+str(res[0])+"\n")
	fline   += [res[0][2]]

fres	+= [fline]
transpose = [] 

for j in range(len(fres[0])):
	transpose += [[]]
	for i in range(len(fres)):
		transpose[j] += [[]]

for i in range(len(fres)):
	for j in range(len(fres[0])):
		transpose[j][i] = str(fres[i][j])
		
h10 = ["ICPE"]
h11 = ["baseline"]
for line in open("phold_cap.dat").readlines():
	line = line.strip()
	if "PL"+str(powercap) in line:
		line = line.split()
		if line[3] == "H10":
			h10 += [line[4]]
		else:
			h11 += [line[4]]

for line in open("traffic_cap.dat").readlines():
	line = line.strip()
	if "PL"+str(powercap) in line:
		line = line.split()
		if line[3] == "H10":
			h10 += [line[4]] 
		else:
			h11 += [line[4]]

for line in open("phold_hs_mixed_cap.dat").readlines():
	line = line.strip()
	if "PL"+str(powercap) in line:
		line = line.split()
		if line[4] == "H10":
			h10 += [line[5]] 
		else:
			h11 += [line[5]]


count = 0
for i in transpose:
	print " ".join(i)+" "+h10[count]+" "+h11[count]
	count+=1
	
