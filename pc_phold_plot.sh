#!/bin/bash

source ./pc_phold_config.sh


mkdir -p ${FOLDER}
mkdir -p ${RES_FOLDER}

for max_lp in $MAX_SKIPPED_LP_list
do
for ck in $CKP_PER_list
do
for pub in $PUB_list
do
for epb in $EPB_list
do
for loop_count in $LOOP_COUNT_list
do
for fan_out in $FAN_OUT_list
do
for lookahead in $LOOKAHEAD_list
do
	for test in $TEST_list 
	do
		for lp in $LP_list 
		do
			FILE1="${RES_FOLDER}/TH-${test}-lf-dymelor-hijacker-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-fan-$fan_out-loop-$loop_count"; touch $FILE1
			FILE2="${RES_FOLDER}/PO-${test}-lf-dymelor-hijacker-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-fan-$fan_out-loop-$loop_count"; touch $FILE2
			line1="THREADS "
			line2="THREADS "
			echo $FILE1
			gnuplot -e "file=\"$FILE1\"" th.gp
			gnuplot -e "file=\"$FILE2\"" po.gp
		done
	done
done
done
done
done
done
done
done
