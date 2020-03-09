#!/bin/bash

MAX_SKIPPED_LP_list="1000000"
LP_list="36 64 121"							#numero di lp
THREAD_list="4 8 16 24 32" #"4 8 12 16 20 24 28 32"		#numero di thread
TEST_list="phold_multicast"						#test
RUN_list="1"							#lista del numero di run

LOOKAHEAD_list="0" 						#lookahead
LOOP_COUNT_list="33 100 300 900 2700" 	#loop_count #50 100 150 250 400 600" 400=60micsec
PROB_list="0.0 0.25 0.5 0.75 1.0"

CKP_PER_list="10"

PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"
TEST_DURATION="60"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/results_phold_multicast" 

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
for prob in $PROB_list
do
for lookahead in $LOOKAHEAD_list
do
	for test in $TEST_list 
	do
		echo make $test NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} THR_PROB_NORMAL=${prob} LOOP_COUNT=${loop_count} PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 CHECKPOINT_PERIOD=${ck} PRINT_SCREEN=0
			 make $test NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} THR_PROB_NORMAL=${prob} LOOP_COUNT=${loop_count} PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 CHECKPOINT_PERIOD=${ck} PRINT_SCREEN=0
		
		mv $test ${test}_ori
		
		echo make $test NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} THR_PROB_NORMAL=${prob} LOOP_COUNT=${loop_count} PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 CHECKPOINT_PERIOD=${ck} PRINT_SCREEN=0 IPI_SUPPORT=1 POSTING=1 SYNCH_CHECK=1 DECISION_MODEL=1
			 make $test NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} THR_PROB_NORMAL=${prob} LOOP_COUNT=${loop_count} PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 CHECKPOINT_PERIOD=${ck} PRINT_SCREEN=0 IPI_SUPPORT=1 POSTING=1 SYNCH_CHECK=1 DECISION_MODEL=1
		
		mv $test ${test}_ipi
		
		for run in $RUN_list
		do
			for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						EX="./${test}_ori $threads $lp $TEST_DURATION"
						FILE="${FOLDER}/${test}-ori-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-prob-$prob-loop-$loop_count-$run"; touch $FILE
												
						N=0 
						while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
						do
							echo $BEGIN
							echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
							echo $FILE
							$EX &> $FILE
							if test $N -ge $MAX_RETRY ; then echo break; break; fi
							N=$(( N+1 ))
						done  
						
						EX="./${test}_ipi $threads $lp $TEST_DURATION"
						FILE="${FOLDER}/${test}-ipi-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-prob-$prob-loop-$loop_count-$run"; touch $FILE
												
						N=0 
						while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
						do
							echo $BEGIN
							echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
							echo $FILE
							$EX &> $FILE
							if test $N -ge $MAX_RETRY ; then echo break; break; fi
							N=$(( N+1 ))
						done  
						  
					done
				done
		done
		rm ${test}_ori ${test}_ipi
	done
done
done
done
done
done
done
done
