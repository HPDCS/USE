echo -n "generating figures 8..."
python3 plot-scripts/figure8v3_plot.py  results/pcs-0.48/ pcs
python3 plot-scripts/figure8v3_plot.py  results/pcs-0.24/ pcs
python3 plot-scripts/figure8v3_plot.py  results/pcs-0.12/ pcs

echo "Done!"
