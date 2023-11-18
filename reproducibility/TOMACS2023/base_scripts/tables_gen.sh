echo -n "generating data for table 2 and 3..."
python3 plot-scripts/process_use_locality_folder_for_tables.py results/pcs-0.48/ >  data/table2.csv
python3 plot-scripts/process_use_locality_folder_for_tables.py results/pcs-0.24/ >> data/table2.csv
python3 plot-scripts/process_use_locality_folder_for_tables.py results/pcs-0.12/ >> data/table2.csv
echo "Done!"

echo -n "generating table 2 ..."
python generate_table2.py > figures/table2.tex
echo "Done!"

echo -n "generating table 3 ..."
python generate_table3.py > figures/table3.tex
echo "Done!"


echo -n "generating data for table 4 and 5..."
python3 plot-scripts/process_use_locality_folder_for_tables.py results/results_tuberculosis/ > data/table4.csv
echo "Done!"


echo -n "generating table 4 ..."
python generate_table4.py > figures/table4.tex
echo "Done!"

echo -n "generating table 5 ..."
python generate_table5.py > figures/table5.tex
echo "Done!"