set terminal postscript eps size 10,10 enhanced color font 'cmu10, 22'
set output file.".eps"
set key inside right top vertical Right noreverse enhanced autotitles columnhead nobox

plot for [col=2:14] file u 1:(column(col))  w lp ls col 

#t word(titles, col) 
system('epstopdf '.file.".eps")
