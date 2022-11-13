mkdir -p data
mkdir -p figures

echo -n "generating data for figure 7..."
python3 plot-scripts/process_use_locality_folder.py results_new_obj_0.48/  > data/figure7.csv
echo "Done!"
echo -n "generating figure 7..."
python3 plot-scripts/figure7_plot.py
echo "Done!"

echo -n "generating data for figure 8..."
python3 plot-scripts/process_use_locality_folder.py results_new_obj_0.24/  > data/figure8.csv
echo "Done!"
echo -n "generating figure 8..."
python3 plot-scripts/figure8_plot.py
echo "Done!"

python plot-scripts/new_plot.py results_new_obj_v2_0.48/ figure9
python plot-scripts/new_plot.py results_new_obj_v2_0.24/ figure10
cd figures
pdflatex merge.tex
cd ..