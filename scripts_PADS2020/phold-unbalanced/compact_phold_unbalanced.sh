#!/bin/bash

LP_list="121 256"						#numero di lp
THREAD_list="8 16 32"					#numero di thread
TEST_list="phold_unbalanced"			#test
RUN_list="1"							#lista del numero di run

FAN_OUT_list="75 100"					#lista fan out
LOOP_COUNT_list="1000"					#loop_count
TOKEN_FACTOR_list="100 10000"			#moltiplicatore token loop_count

SIM_list="original ipi"

SRC_FOLDER="results/results_phold_unbalanced" 
FOLDER="results/results_phold_unbalanced/dat" 

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
			line="THREADS "
			for sim in $SIM_list
			do
				line="$line $sim"
			done
			OUT="${FOLDER}/${test}-lps-$lp-fan-$fan_out-loop-$loop_count-tk_fctr-$token_factor"
			echo $line > $OUT
			for threads in $THREAD_list
			do
				line="$threads "
				
				for sim in $SIM_list
				do
					th=0
					count=0
					sum=0
					for run in $RUN_list
					do
						FILE="${SRC_FOLDER}/${test}-$sim-$threads-$lp-fan-$fan_out-loop-$loop_count-$token_factor"; 
						th=`grep "EventsPerSec"     $FILE | cut -f2 -d':'`
						th=`python2 -c "print '$th'.strip()"`
						sum=`python2 -c "print $th+$sum"`
						count=$(($count+1))
					done
					avg=`python2 -c "print $sum/$count"`
					line="$line $avg"
				done
				echo $line >> $OUT
			done
		done
	done
done
done
done