set terminal postscript eps size 10,10 enhanced color font 'cmu10, 22'
set output file.".eps"
set title tit
set key inside right bottom vertical Right noreverse enhanced autotitles columnhead nobox

set size 0.5,0.3
set yrange [0:*]
plot for [col=2:3] file u 1:(column(col))  w lp ls col 

#t word(titles, col) 
system('epstopdf '.file.".eps")
