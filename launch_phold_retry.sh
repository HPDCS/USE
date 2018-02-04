#!/bin/bash

MAX_SKIPPED_LP_list="1000000"
LP_list="1024"						#numero di lp
THREAD_list="2 4 8 16 24 32" #"4 8 16 24 32"		#numero di thread
TEST_list="phold"					#test
RUN_list="1"				#lista del numero di run

FAN_OUT_list="1" # 50"				#lista fan out
LOOKAHEAD_list="0 0.01" #"0 0.1 0.01"		#lookahead
LOOP_COUNT_list="50 150 400 800 1600" 				#loop_count #50 100 150 250 400 600" 400=60micsec

CKP_PER_list="50" #"10 50 100"

PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"
TEST_DURATION="20"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/results_phold" #/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

mkdir -p results
mkdir -p results/results_phold
#mkdir ${FOLDER}

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
		echo make $test NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}  PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 CKP_PERIOD=${ck} PRINT_SCREEN=0
		make $test NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}  PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 CKP_PERIOD=${ck} PRINT_SCREEN=0
		mv $test ${test}_lf_hi
		
		for run in $RUN_list
		do
			for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						EX="./${test}_lf_hi $threads $lp $TEST_DURATION"
						FILE="${FOLDER}/${test}-lf-dymelor-hijacker-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-fan-$fan_out-loop-$loop_count-$run"; touch $FILE
												
						N=0 
						while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
						do
							echo $BEGIN
							echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
							echo $FILE
							#1> $FILE 2>&1 time $EX
							(time $EX) &> $FILE
							if test $N -ge $MAX_RETRY ; then echo break; break; fi
							N=$(( N+1 ))
						done  
						
						  
					done
				done
		done
		rm ${test}_lf_hi
	done
done
done
done
done
done
done
done
