echo -n "generating data for figure 7 and 8..."
python3 scripts_plot/process_use_locality_folder.py results/results_phold/ > data/figure7.csv
echo "Done!"
echo -n "generating figure 7..."
python3 scripts_plot/figure7_plot.py
echo "Done!"

echo -n "generating figure 8..."
python3 scripts_plot/figure8_plot.py
echo "Done!"
