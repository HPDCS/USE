#!/bin/bash

source $1
source $2

cd ../..
mkdir -p ${PLT_FOLDER}


for max_lp in $MAX_SKIPPED_LP_list
do
for ck in $CKP_PER_list
do
for pub in $PUB_list
do
for epb in $EPB_list
do
for ta in $TA_list
do
for ta_duration in $TA_DURATION_list
do
for channels_per_cell in $CHANNELS_PER_CELL_list
do
for ta_change in $TA_CHANGE_list
do
for lookahead in $LOOKAHEAD_list
do
	for test in $TEST_list 
	do
		for lp in $LP_list
		do
			OUT="${DAT_FOLDER}/${test}-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-ta-$ta-ta_duration-$ta_duration-chan_per_cell-$channels_per_cell-ta_change-$ta_change"; 				
			gnuplot -e "file=\"$OUT\"" -e "tit=\"LP:$lp-TA:$ta-TAD:$ta_duration-CH:$channels_per_cell-TAC:$ta_change-CKP:$ck\"" th.gp
			cp $OUT.pdf ${PLT_FOLDER}
			rm $OUT.eps
			rm $OUT.pdf
		done
	done
done
done
done
done
done
done
done
done
done

file_list=$(ls ${PLT_FOLDER})
file_list2=""
for file in ${file_list}
do
	file_list2+="${PLT_FOLDER}/${file} "
done

pdfunite ${file_list2} ${FOLDER}/${test}_unito.pdf

cd scripts_PADS2020/pcs
