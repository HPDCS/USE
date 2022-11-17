mkdir -p data
mkdir -p figures


echo -n "generating data for figure 5..."
python3 plot-scripts/process_use_locality_folder.py results/pcs_static_win/  > data/figure5.csv
echo "Done!"
echo -n "generating figure 5..."
python3 plot-scripts/figure5_plot.py
echo "Done!"

echo -n "generating data for figure 6..."
python3 plot-scripts/process_use_locality_folder.py results/pcs_win_explore/ > data/figure6.csv
echo "Done!"
echo -n "generating figure 6..."
python3 plot-scripts/figure6_plot.py
echo "Done!"

echo -n "generating data for figure 7..."
python3 plot-scripts/process_use_locality_folder.py results/results_phold/   > data/figure7.csv
echo "Done!"
echo -n "generating figure 7..."
python3 plot-scripts/figure7_plot.py
echo "Done!"

echo -n "generating figure 8a..."
python plot-scripts/figure8_plot.py results_new_obj_v2_0.48/ figure9
echo "Done!"
echo -n "generating figure 8b..."
python plot-scripts/figure8_plot.py results_new_obj_v2_0.24/ figure10
echo "Done!"
echo -n "generating figure 8c..."
python plot-scripts/figure8_plot.py results_new_obj_v2_0.12/ figure11
echo "Done!"

echo -n "generating figure 9a..."
python plot-scripts/figure9a_plot.py results_new_obj_v2_0.48/ figure9
echo "Done!"


echo -n "generating figure 9b..."
python plot-scripts/figure9b_plot.py results_new_obj_v2_0.48/ figure9
echo "Done!"



cd figures
#pdflatex merge.tex
cd ..