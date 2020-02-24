#!/bin/bash

LP_list="36 64 121"						#numero di lp
THREAD_list="4 8 16 24 32"				#numero di thread
TEST_list="phold_unbalanced"			#test
RUN_list="1"							#lista del numero di run

FAN_OUT_list="25 50 75 100"				#lista fan out
LOOP_COUNT_list="10 50 100"				#loop_count
TOKEN_FACTOR_list="100 10000 1000000"	#moltiplicatore token loop_count

FOLDER="results/results_phold_unbalanced/results_phold_unbalanced_$(date +%Y%m%d)-$(date +%H%M)"

mkdir results
mkdir results/results_phold

mkdir ${FOLDER}

for token_factor in $TOKEN_FACTOR_list
do
for loop_count in $LOOP_COUNT_list
do
for fan_out in $FAN_OUT_list
do
	for test in $TEST_list 
	do
		make -B $test FAN_OUT=${fan_out} TOKEN_LOOP_COUNT_FACTOR=${token_factor} LOOP_COUNT=${loop_count} CHECKPOINT_PERIOD=10
		mv $test ${test}_orig
		
		make -B $test ENABLE_IPI_MODULE=1 FAN_OUT=${fan_out} TOKEN_LOOP_COUNT_FACTOR=${token_factor} LOOP_COUNT=${loop_count} CHECKPOINT_PERIOD=10
		mv $test ${test}_ipi

		for run in $RUN_list
		do
			for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						echo "./${test}_orig $threads $lp 120 > ${FOLDER}/${test}-original-$threads-$lp-fan-$fan_out-loop-$loop_count-$token_factor"
							  ./${test}_orig $threads $lp 120 > ${FOLDER}/${test}-original-$threads-$lp-fan-$fan_out-loop-$loop_count-$token_factor
						echo "./${test}_ipi $threads $lp 120 > ${FOLDER}/${test}-ipi-$threads-$lp-fan-$fan_out-loop-$loop_count-$token_factor"
							  ./${test}_ipi $threads $lp 120 > ${FOLDER}/${test}-ipi-$threads-$lp-fan-$fan_out-loop-$loop_count-$token_factor
					done
				done
		done
		rm ${test}_orig
		rm ${test}_ipi
	done
done
done
done