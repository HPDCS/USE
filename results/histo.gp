set term postscript eps enhanced color font "Courier" 50 size 28,6
set auto x
set style data histogram
set style histogram cluster gap 1
set style fill solid border -1
set boxwidth 0.9
set style line 12 lc rgb '#000000' lt "-" lw 2
set grid y back ls 12

#set xtic rotate by -45 scale 0
#set bmargin 10 
set xtics ("phold-60" 0, "phold-256" 1, "phold-1024" 2, "traffic" 3, "hs-mixed-2" 4 , "hs-mixed-5" 5 )
set size 0.7,0.8

set yrange [0.0:1.3]
#set xrange [0.4:5.6]
set key top left horizontal
set xlabel "benchmarks"
set ylabel "Relative speedup"


set output '65.eps'
set title "Slowdown"
plot '65.dat' using (column(2)/column(2)) ti columnheader(2), '' u (column(3)/column(2)) ti columnheader(3), '' u (column(4)/column(2)) ti columnheader(4)
system('epstopdf 65.eps')


set output '55.eps'
set title "Slowdown"
plot '55.dat' using (column(2)/column(2)) ti columnheader(2), '' u (column(3)/column(2)) ti columnheader(3), '' u (column(4)/column(2)) ti columnheader(4)
system('epstopdf 55.eps')

