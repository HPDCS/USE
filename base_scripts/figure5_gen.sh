mkdir -p data
mkdir -p figures

echo -n "generating data for figure 5..."
python3 plot-scripts/process_use_locality_folder.py results/pcs_static_win/  > data/figure5.csv
echo "Done!"
echo -n "generating figure 5..."
python3 plot-scripts/figure5_plot.py
echo "Done!"
