echo -n "generating figures 9..."
python plot-scripts/figure8v1_plot.py  results_new_obj_0.48/ pcs_hs
python plot-scripts/figure8v2_plot.py  results_new_obj_0.48/ pcs_hs
python plot-scripts/figure8v3_plot.py  results_new_obj_0.48/ pcs_hs

python plot-scripts/figure8v1_plot.py  results_new_obj_0.24/ pcs_hs
python plot-scripts/figure8v2_plot.py  results_new_obj_0.24/ pcs_hs
python plot-scripts/figure8v3_plot.py  results_new_obj_0.24/ pcs_hs

python plot-scripts/figure8v1_plot.py  results_new_obj_0.12/ pcs_hs
python plot-scripts/figure8v2_plot.py  results_new_obj_0.12/ pcs_hs
python plot-scripts/figure8v3_plot.py  results_new_obj_0.12/ pcs_hs



python plot-scripts/figure9_plot.py  results_new_obj_0.48/ pcs
python plot-scripts/figure9_plot.py  results_new_obj_0.24/ pcs
python plot-scripts/figure9_plot.py  results_new_obj_0.12/ pcs

python plot-scripts/figure9_plot.py  results_new_obj_0.48/ pcs_hs
python plot-scripts/figure9_plot.py  results_new_obj_0.24/ pcs_hs
python plot-scripts/figure9_plot.py  results_new_obj_0.12/ pcs_hs






echo "Done!"