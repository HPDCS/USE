#run statiche
#./pc_phold_compact.sh
#./pc_traffic_compact.sh
#./pc_pholdhotspotmixed_compact.sh

#run adaptive
#echo ./pc_compact2_phold_heuristics.sh
#./pc_compact2_phold_heuristics.sh > results/phold_cap.dat
#echo ./pc_compact_traffic_heuristic.sh
#./pc_compact_traffic_heuristic.sh > results/traffic_cap.dat
#echo ./pc_compact_pholdhotspotmixed_heuristics.sh
#./pc_compact_pholdhotspotmixed_heuristics.sh > results/phold_hs_mixed_cap.dat

cd results
python get_static_optimum.py 55.0 > 55.dat
python get_static_optimum.py 65.0 > 65.dat
gnuplot histo.gp
cd ..
