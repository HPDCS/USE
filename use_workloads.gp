set terminal postscript eps size 10,10 enhanced color font 'cmu10, 22'
set output "use_workloads.eps"
set key inside right top vertical Right noreverse enhanced autotitles columnhead nobox
set key inside left top horizontal Right noreverse enhanced autotitles columnhead nobox width 5
set key inside right bottom vertical Right width 0.3 maxrows 5

set grid x,y
set xrange [0:41]
set yrange [-0.05:1.05]
set size 0.6,0.35
set ylabel "Normalized Speed Up \n w.r.t. Sequential Execution"
set xlabel "#Threads"
#set ylabel "AVG Power (W)"

#set logscale y

set samples 40

plot \
	 'results/plots_phold/TH-phold-lf-dymelor-hijacker-60-maxlp-1000000-look-0-ck_per-50-fan-1-loop-500' u 1:( ($2-13687.08)  /(198748.00 - 13687.08)) smooth  acsplines    w lp ls 2 t "phold-60"  , \
    'results/plots_phold/TH-phold-lf-dymelor-hijacker-256-maxlp-1000000-look-0-ck_per-50-fan-1-loop-500' u 1:( ($2-13561.37)  /(371001.30 - 13561.37)) smooth  acsplines    w lp ls 3 t "phold-256" , \
   'results/plots_phold/TH-phold-lf-dymelor-hijacker-1024-maxlp-1000000-look-0-ck_per-50-fan-1-loop-500' u 1:( ($2-13039.94)  /(410499.60 - 13039.94)) smooth  acsplines    w lp ls 4 t "phold-1024", \
   'results/plots_traffic/TH-traffic_137_03_08' 													     u 1:( ($2- 1878.81)  /(  5783.24 -  1878.81)) smooth  acsplines    w lp ls 6 t "traffic", \
   'results/plots_pholdhotspotmixed_th_correct/TH-pholdhotspotmixed-lf-dymelor-hijacker-nhs2-psf0-w999-h8-pop200-pws10-1024-maxlp-1000000-look-0-ck_per-50-fan-5-loop-500' 	u 1:( ($2- 1878.81)  /(  5783.24 -  1878.81)) smooth  acsplines    w lp ls 6 t "phold hotspot 2", \
   'results/plots_pholdhotspotmixed_th_correct/TH-pholdhotspotmixed-lf-dymelor-hijacker-nhs5-psf0-w999-h8-pop200-pws10-1024-maxlp-1000000-look-0-ck_per-50-fan-5-loop-500' 	u 1:( ($2- 1878.81)  /(  5783.24 -  1878.81)) smooth  acsplines    w lp ls 6 t "phold hotspot 5", \
 #   'results/plots_phold/TH-phold-lf-dymelor-hijacker-60-maxlp-1000000-look-0-ck_per-50-fan-1-loop-500' u 1:( ($2-13687.08)  /(198748.00 - 13687.08))  w lp ls 2 t "phold-60"  , \
    'results/plots_phold/TH-phold-lf-dymelor-hijacker-256-maxlp-1000000-look-0-ck_per-50-fan-1-loop-500' u 1:( ($2-13561.37)  /(371001.30 - 13561.37))  w lp ls 3 t "phold-256" , \
   'results/plots_phold/TH-phold-lf-dymelor-hijacker-1024-maxlp-1000000-look-0-ck_per-50-fan-1-loop-500' u 1:( ($2-13039.94)  /(410499.60 - 13039.94))  w lp ls 4 t "phold-1024", \
   'results/plots_traffic/TH-traffic_137_03_08' 													     u 1:( ($2- 1878.81)  /(  5783.24 -  1878.81))  w lp ls 6 t "traffic"



#t word(titles, col) 
system('epstopdf '.'use_workloads'.".eps")
