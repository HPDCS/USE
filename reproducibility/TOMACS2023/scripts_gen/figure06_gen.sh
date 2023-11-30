#!/bin/bash

source scripts_gen/common_gen.sh

echo -n "generating data for figure 6..."
python3 scripts_plot/process_use_locality_folder.py $resfolder/pcs_win_explore/ > data/figure6.csv
echo "Done!"
echo -n "generating figure 6..."
python3 scripts_plot/figure6_plot.py
echo "Done!"
