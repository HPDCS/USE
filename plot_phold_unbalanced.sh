#!/bin/bash

LP_list="121 256"						#numero di lp
THREAD_list="8 16 32"					#numero di thread
TEST_list="phold_unbalanced"			#test
RUN_list="1"							#lista del numero di run

FAN_OUT_list="75 100"					#lista fan out
LOOP_COUNT_list="1000"					#loop_count
TOKEN_FACTOR_list="100 10000"			#moltiplicatore token loop_count

SRC_FOLDER="results/results_phold_unbalanced/dat" 
FOLDER="results/results_phold_unbalanced/plots"

mkdir -p ${FOLDER}

for token_factor in $TOKEN_FACTOR_list
do
for loop_count in $LOOP_COUNT_list
do
for fan_out in $FAN_OUT_list
do
	for test in $TEST_list 
	do
		for lp in $LP_list
		do
			OUT="${SRC_FOLDER}/${test}-lps-$lp-fan-$fan_out-loop-$loop_count-tk_fctr-$token_factor"
			gnuplot -e "file=\"$OUT\"" -e "tit=\"${test}-lps-$lp-fan-$fan_out-loop-$loop_count-tk_fctr-$token_factor-ckp-10\"" th.gp
			cp $OUT.pdf ${FOLDER}
			rm $OUT.eps
			rm $OUT.pdf
		done
	done
done
done
done