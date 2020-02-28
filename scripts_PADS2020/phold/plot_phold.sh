#!/bin/bash

MAX_SKIPPED_LP_list="1000000"
LP_list="36 64 121"							#numero di lp
THREAD_list="4 8 12 16 20 24 28 32"		#numero di thread
TEST_list="phold"						#test
RUN_list="1 2"							#lista del numero di run

FAN_OUT_list="1"						#lista fan out
LOOKAHEAD_list="0" 						#lookahead
LOOP_COUNT_list="33 100 300 900 2700" 	#loop_count #50 100 150 250 400 600" 400=60micsec

CKP_PER_list="10"

PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"
TEST_DURATION="60"
SIM_list="ori ipi"

SRC_FOLDER="results/results_phold/dat" 
FOLDER="results/results_phold/plots" 

mkdir -p ${FOLDER}

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
				OUT="${SRC_FOLDER}/${test}-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-fan-$fan_out-loop-$loop_count"
				gnuplot -e "file=\"$OUT\"" -e "tit=\"LP:$lp-FAN:$fan_out-LOOP:$loop_count-CKP:$ck\"" th.gp
				cp $OUT.pdf ${FOLDER}
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
