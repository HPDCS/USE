#!/bin/bash

source scripts_gen/common_gen.sh

echo -n "generating figures 12..."
python3 scripts_plot/figuretuberculosis_plot.py $resfolder/results_tuberculosis/ tuberculosis full
echo "Done!"
