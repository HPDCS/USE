echo -n "generating figures 9..."
python plot-scripts/figure8v1_plot.py  results/pcs-hs-0.48/ pcs_hs
python plot-scripts/figure8v2_plot.py  results/pcs-hs-0.48/ pcs_hs
python plot-scripts/figure8v3_plot.py  results/pcs-hs-0.48/ pcs_hs
python plot-scripts/figure8v1_plot.py  results/pcs-hs-0.24/ pcs_hs
python plot-scripts/figure8v2_plot.py  results/pcs-hs-0.24/ pcs_hs
python plot-scripts/figure8v3_plot.py  results/pcs-hs-0.24/ pcs_hs
python plot-scripts/figure8v1_plot.py  results/pcs-hs-0.12/ pcs_hs
python plot-scripts/figure8v2_plot.py  results/pcs-hs-0.12/ pcs_hs
python plot-scripts/figure8v3_plot.py  results/pcs-hs-0.12/ pcs_hs

python plot-scripts/figure9_plot.py    results/pcs-hs-0.48/ pcs_hs
#python plot-scripts/figure9_plot.py    results/pcs-hs-0.24/ pcs_hs
#python plot-scripts/figure9_plot.py    results/pcs-hs-0.12/ pcs_hs






echo "Done!"