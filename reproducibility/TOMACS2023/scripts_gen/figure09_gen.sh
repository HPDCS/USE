echo -n "generating figures 9a..."
python3 scripts_plot/figure9_plot.py  results/pcs-0.48/ pcs
echo "Done!"

echo -n "generating figures 9b..."
python3 scripts_plot/figure9_plot.py  results/pcs-0.24/ pcs
echo "Done!"

echo -n "generating figures 9c..."
python3 scripts_plot/figure9_plot.py  results/pcs-0.12/ pcs
echo "Done!"