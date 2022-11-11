mkdir -p data
mkdir -p figures

echo -n "generating data for figure 7..."
python3 plot-scripts/process_use_locality_folder.py results_new_obj/  > data/figure7.csv
echo "Done!"
echo -n "generating figure 7..."
python3 plot-scripts/figure7_plot.py
echo "Done!"

echo -n "generating data for figure 8..."
python3 plot-scripts/process_use_locality_folder.py results_new_obj_0.2/  > data/figure8.csv
echo "Done!"
echo -n "generating figure 8..."
python3 plot-scripts/figure8_plot.py
echo "Done!"

