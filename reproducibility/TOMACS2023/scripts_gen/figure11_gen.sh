#python3 scripts_plot/figuretuberculosis_plot.py results/results_tuberculosis/ tuberculosis
echo -n "generating figures 11a..."
python3 scripts_plot/figure9_plot.py  results/pcs-dynhot pcs_hs_dyn
echo "Done!"

echo -n "generating figures 11b..."
python3 scripts_plot/figure10_plot.py results/pcs-dynhot pcs_hs_dyn
echo "Done!"

echo -n "generating figures 11c..."
python3 scripts_plot/figure11c_plot.py  results/pcs-dynhot pcs_hs_dyn
echo "Done!"

echo -n "generating figures 11d..."
python3 scripts_plot/figure11d_plot.py  results/pcs-dynhot pcs_hs
echo "Done!"


