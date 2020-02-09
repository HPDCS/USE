#!/bin/bash

MAX_SKIPPED_LP_list="1000000"
LP_list="1024 512 256 80 60"							#numero di lp
THREAD_list="1 5 10 15 20 25 30 35 40"	#numero di thread
TEST_list="phold"						#test
RUN_list="1"							#lista del numero di run

FAN_OUT_list="1" 						#lista fan out
LOOKAHEAD_list="0"		 				#lookahead
LOOP_COUNT_list="500" 					#loop_count 

CKP_PER_list="50"
PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"
TEST_DURATION="10"

PSTATE_list="1 2 3 4 5 6 7 8 9 10 11 12 13 14"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/results_phold"
RES_FOLDER="results/plots_phold"
HEURISTIC_MODE=8
POWER_LIMIT=65.0
STATIC_PSTATE=1
CORES=40


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
			for pstate in $PSTATE_list
			do
				line1="$line1 P$pstate"
				line2="$line2 P$pstate"
			done
			echo $line1 > $FILE1
			echo $line2 > $FILE2
			for threads in $(seq $CORES)
			do
				line1="$threads "
				line2="$threads "
				for pstate in $PSTATE_list
				do
					for run in $RUN_list
					do
						FILE="${FOLDER}/${test}-lf-dymelor-hijacker-$threads-p$pstate-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-fan-$fan_out-loop-$loop_count-$run"; touch $FILE
						th=`grep "EventsPerSec"     $FILE | cut -f2 -d':'`
						po=`grep "Power consumption (net value)"  $FILE | cut -f2 -d':'`
						th=`python -c "print '$th'.strip()"`
						po=`python -c "print '$po'.strip()"`
					done
				line1="$line1 $th"
				line2="$line2 $po"
				done				
				echo $line1 >> $FILE1
				echo $line2 >> $FILE2
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
