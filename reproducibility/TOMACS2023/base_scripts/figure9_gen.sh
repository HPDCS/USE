echo -n "generating figures 9..."
python3 plot-scripts/figure9_plot.py  results/pcs-0.48/ pcs
python3 plot-scripts/figure9_plot.py  results/pcs-0.24/ pcs
python3 plot-scripts/figure9_plot.py  results/pcs-0.12/ pcs

echo "Done!"
