set terminal postscript eps size 10,10 enhanced color font 'cmu10, 22'
set output file.".eps"
set key inside right top vertical Right noreverse enhanced autotitles columnhead nobox
set key inside left top horizontal Right noreverse enhanced autotitles columnhead nobox width 5
set key inside left top horizontal Right width 0.5 maxcolumns 3

set grid x,y
set xrange [0:41]
set yrange [30:100]
set size 0.6,0.35
#set ylabel "Throughput (Committed Evts per Sec)"
set ylabel "AVG Power (W)"
set xlabel "#Threads"

set samples 40

plot for [col=2:14] file u 1:(column(col)) smooth  acsplines  w lp ls col 

#t word(titles, col) 
system('epstopdf '.file.".eps")
