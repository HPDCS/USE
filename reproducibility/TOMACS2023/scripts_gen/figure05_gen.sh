echo -n "generating data for figure 5..."
python3 scripts_plot/process_use_locality_folder.py results/pcs_static_win/  > data/figure5.csv
echo "Done!"
echo -n "generating figure 5..."
python3 scripts_plot/figure5_plot.py
echo "Done!"
