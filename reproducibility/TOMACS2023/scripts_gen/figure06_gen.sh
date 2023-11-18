echo -n "generating data for figure 6..."
python3 scripts_plot/process_use_locality_folder.py results/pcs_win_explore/ > data/figure6.csv
echo "Done!"
echo -n "generating figure 6..."
python3 scripts_plot/figure6_plot.py
echo "Done!"
