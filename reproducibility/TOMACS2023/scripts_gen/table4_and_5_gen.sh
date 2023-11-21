echo -n "generating data for table 4 and 5..."
python3 scripts_table/process_use_locality_folder_for_tables.py results/results_tuberculosis/ > data/table4.csv
echo "Done!"


echo -n "generating table 4 ..."
python3 scripts_table/generate_table4.py > figures_reproduced/table4.tex
echo "Done!"

echo -n "generating table 5 ..."
python3 scripts_table/generate_table5.py > figures_reproduced/table5.tex
echo "Done!"
