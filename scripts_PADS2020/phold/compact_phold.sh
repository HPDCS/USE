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

SRC_FOLDER="results/results_phold" 
FOLDER="results/results_phold/dat" 

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
				line="THREADS "
				for sim in $SIM_list
				do
				 line="$line $sim"
				done
				OUT="${FOLDER}/${test}-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-fan-$fan_out-loop-$loop_count"
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
							FILE="${SRC_FOLDER}/${test}-$sim-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-fan-$fan_out-loop-$loop_count-$run"; 
							th=`grep "EventsPerSec"     $FILE | cut -f2 -d':'`
							th=`python -c "print '$th'.strip()"`
							sum=`python -c "print $th+$sum"`
							count=$(($count+1))
						done
						avg=`python -c "print $sum/$count"`
						line="$line $avg"
					done
					echo $line >> $OUT
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
