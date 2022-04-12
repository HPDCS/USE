mkdir -p data
mkdir -p figures


echo -n "generating data for figure 3..."
python3 plot-scripts/process_use_locality_folder.py results/pcs_static_win/  > data/figure3.csv
echo "Done!"

echo -n "generating data for figure 4..."
python3 plot-scripts/process_use_locality_folder.py results/pcs_win_explore/ > data/figure4.csv
echo "Done!"

echo -n "generating data for figure 5..."
python3 plot-scripts/process_use_locality_folder.py results/results_phold/   > data/figure5.csv
echo "Done!"
echo -n "generating figure 5..."
python3 plot-scripts/figure5_plot.py
echo "Done!"

echo -n "generating data for figure 6..."
python3 plot-scripts/process_use_locality_folder.py results/pcs_dyn_win/     > data/figure6.csv
echo "Done!"




