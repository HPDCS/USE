echo -n "generating figures 8..."
python plot-scripts/figure8v3_plot.py  results/pcs-0.48/ pcs
python plot-scripts/figure8v3_plot.py  results/pcs-0.24/ pcs
python plot-scripts/figure8v3_plot.py  results/pcs-0.12/ pcs

echo "Done!"