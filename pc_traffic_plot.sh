#!/bin/bash

source ./pc_traffic_config.sh


mkdir -p ${FOLDER}
mkdir -p ${RES_FOLDER}






for fn in $INPUT_list 
do

	for lp in $LP_list
	do
		FILE1="${RES_FOLDER}/TH-traffic_${lp}_${fn}"; touch $FILE1
		FILE2="${RES_FOLDER}/PO-traffic_${lp}_${fn}"; touch $FILE2

		line1="THREADS "
		line2="THREADS "
		echo $FILE1
		gnuplot -e "file=\"$FILE1\"" th.gp
		gnuplot -e "file=\"$FILE2\"" po.gp

	done
done
