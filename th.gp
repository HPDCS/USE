set terminal postscript eps size 10,10 enhanced color font 'cmu10, 22'
set output file.".eps"
set key inside right top vertical Right noreverse enhanced autotitles columnhead nobox
set key inside left top horizontal Right noreverse enhanced autotitles columnhead nobox width 5
set key inside right bottom vertical Right width 0.3 maxrows 5

set grid x,y
set xrange [0:41]
set size 0.6,0.35
set ylabel "Speed Up w.r.t. P13 and 1 thread"
set xlabel "#Threads"
#set ylabel "AVG Power (W)"

set samples 40

plot for [col=2:14] file u 1:(column(col)) smooth  acsplines w lp ls col 

#t word(titles, col) 
system('epstopdf '.file.".eps")
