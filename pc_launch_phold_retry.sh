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

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/results_phold"
PSTATE_list="1 2 3 4 5 6 7 8 9 10 11 12 13 14"
HEURISTIC_MODE=8
POWER_LIMIT=65.0
STATIC_PSTATE=1
CORES=40


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
		echo make $test POWERCAP=1 NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}  PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 CKP_PERIOD=${ck} PRINT_SCREEN=0
		make $test POWERCAP=1 NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}  PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 CKP_PERIOD=${ck} PRINT_SCREEN=0
		mv $test ${test}_lf_hi
		
		for run in $RUN_list
		do
			for lp in $LP_list
				do
					for pstate in $PSTATE_list
					do
						for threads in $(seq $CORES) #$THREAD_list
						do
						
							./create_config.sh $threads $pstate $HEURISTIC_MODE $POWER_LIMIT
							EX="sudo ./${test}_lf_hi $CORES $lp $TEST_DURATION"
							FILE="${FOLDER}/${test}-lf-dymelor-hijacker-$threads-p$pstate-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-fan-$fan_out-loop-$loop_count-$run"; touch $FILE
													
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
