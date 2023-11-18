echo -n "generating data for table 2 and 3..."
python3 scripts_table/process_use_locality_folder_for_tables.py results/pcs-0.48/ >  data/table2.csv
python3 scripts_table/process_use_locality_folder_for_tables.py results/pcs-0.24/ >> data/table2.csv
python3 scripts_table/process_use_locality_folder_for_tables.py results/pcs-0.12/ >> data/table2.csv
echo "Done!"

echo -n "generating table 2 ..."
python scripts_table/generate_table2.py > figures_reproduced/table2.tex
echo "Done!"

echo -n "generating table 3 ..."
python scripts_table/generate_table3.py > figures_reproduced/table3.tex
echo "Done!"
