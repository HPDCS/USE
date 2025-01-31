import sys


translate = {
	"pcs" : {
		'0.1' : 'heavy',
		'0.2' : 'strong',
		'0.8' : 'lightweight',
	},
	"highway" : {
		'1.5' : 'heavy',
		'1.0' : 'strong',
		'0.25' : 'lightweight',
	},
}


results = {}

for type in ['committed', 'executed']:
	results[type] = {}
	for unit in ['ppc', 'x86']:
		results[type][unit] = {}
		for test in ['pcs', 'highway', 'highway-unbalanced']:
			results[type][unit][test] = {}
			for load in ['heavy', 'strong', 'lightweight']:
				results[type][unit][test][load] = {}
				for th in [8,16,24,32,40,48,56,64,72,80,88,96]:
					results[type][unit][test][load][th] = [0]

import os.path

for type in ['committed', 'executed']:
	for unit in ['ppc', 'x86']:
		if not os.path.isfile(f"{unit}_{type}.dat"): continue
		f = open(f"{unit}_{type}.dat")
		for line in f.readlines():
			if "seq" in line: continue 
			test, data = line.strip().replace('./', '').split('/')
			test_fam, load = test.split('-')[0], test.split('-')[-1]
			load = translate[test_fam][load]
			if 'unbalanced' in test:
				test_fam += '-unbalanced'
			throughput = float(data.split(':')[-1])
			threads = int(data.split('-')[1])
			results[type][unit][test_fam][load][threads] += [throughput]
		f.close()
		

labels = {
	'pcs' : "PCS",
	'highway':"Highway", 
	'highway-unbalanced':"Unbalanced Highway"
}

for test in ['pcs', 'highway', 'highway-unbalanced']:
	for load in ['heavy', 'strong', 'lightweight']:
		f = open(f"{test}-{load}.dat", "w")
		for th in [8,16,24,32,40,48,56,64,72,80,88,96]:
			tmp = []
			for unit in ['ppc', 'x86']:
				for type in ['executed', 'committed']:
					avg = sum(results[type][unit][test][load][th])/len(results[type][unit][test][load][th])
					if type == 'executed' : avg /= 60
					tmp+=[avg]
			f.write(f"{th}\t{tmp[0]}\t{tmp[1]}\t{tmp[2]}\t{tmp[3]}\n")
		f.close()

		f = open(f"{test}-{load}.gp", "w")
		f.write(f"""set terminal pngcairo size 800,600 enhanced
		set output 'use-{test}-{load}.png'
		set xlabel "#CPUs"
		set ylabel "Throughput (events per sec.)"
		set title "{labels[test]} {load} - USE"
		set grid ytics

		set style data histogram
		set style fill solid 1.0 border rgb "black"  # Yellow bars
		set boxwidth 0.6

		set yrange [000000:*]


		# Let Gnuplot automatically assign colors
		plot "{test}-{load}.dat" \
		   using 2:xtic(1) with histogram title "powerPC RISC (total)" lc rgb "orange", \
		"" using 3:xtic(1) with histogram title "committed" lc rgb "orange" fillstyle pattern 6, \
		"" using 4:xtic(1) with histogram title "x86 CISC (total)" lc rgb "green", \
		"" using 5:xtic(1) with histogram title "committed" lc rgb "green" fillstyle pattern 6
		""")
		f.close()

		import subprocess
		subprocess.run(["gnuplot", f"{test}-{load}.gp"]) 
