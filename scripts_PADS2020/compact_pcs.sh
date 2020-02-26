#!/bin/bash

MAX_SKIPPED_LP_list="1000000"
LP_list="36 64 121"							#numero di lp
THREAD_list="4 8 16 24 32" #"4 8 12 16 20 24 28 32"		#numero di thread
TEST_list="pcs"						#test
RUN_list="1"							#lista del numero di run

LOOKAHEAD_list="0" 						#lookahead

TA_list="0.9 0.3 0.09 0.03"
TA_DURATION_list="120"
CHANNELS_PER_CELL_list="500"
TA_CHANGE_list="50.0 20.0 10.0"

CKP_PER_list="5 10"

PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"
TEST_DURATION="60"
SIM_list="ori ipi"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

SRC_FOLDER="results/results_pcs" 
FOLDER="results/results_pcs/dat" 

mkdir -p ${FOLDER}

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
			line="THREADS "
			for sim in $SIM_list
			do
			 line="$line $sim"
			done
			
			OUT="${FOLDER}/${test}-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-ta-$ta-ta_duration-$ta_duration-chan_per_cell-$channels_per_cell-ta_change-$ta_change"; 				
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
						FILE="${SRC_FOLDER}/${test}-$sim-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-ta-$ta-ta_duration-$ta_duration-chan_per_cell-$channels_per_cell-ta_change-$ta_change-$run"; 			
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
done
done
