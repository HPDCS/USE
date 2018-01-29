#!/bin/bash

LP_list="1024"						#numero di lp
THREAD_list="1 4 8 16 24 32"		#numero di thread
TEST_list="tcar"					#test
RUN_list="1 2 3 4 5"				#lista del numero di run

LOOP_COUNT_list="400" #4500 1000"	#loop_count
LOOKAHEAD_list="0 0.01 0.05 0.1"			#lookahead
ROB_PER_CELLA_list="2"				#robot per cella
NUM_CELLE_OCC="80 160"				#numero di celle occupate

CKP_PER_list="10 50 5 1"

PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/results_tcar" #/results_tcar_$(date +%Y%m%d)-$(date +%H%M)"

mkdir results
mkdir results/results_tcar
mkdir ${FOLDER}

for pub in $PUB_list
do
for epb in $EPB_list
do
for ck in $CKP_PER_list
do
for loop_count in $LOOP_COUNT_list
do
for lookahead in $LOOKAHEAD_list
do
for robot_per_cella in $ROB_PER_CELLA_list
do
for num_celle_occupate in $NUM_CELLE_OCC
do

	for test in $TEST_list
	do
		make $test NBC=1 REVERSIBLE=0 LOOKAHEAD=${lookahead} NUM_CELLE_OCCUPATE=${num_celle_occupate} ROBOT_PER_CELLA=${robot_per_cella} LOOP_COUNT=${loop_count} PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=0 DEBUG=0 CKP_PERIOD=${ck}
		mv $test ${test}_lf_hi
		for run in $RUN_list 
		do
			for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						EX="./${test}_lf_hi $threads $lp"
						FILE="${FOLDER}/${test}-lf-dymelor-hijacker-$threads-$lp-look-$lookahead-ck_per-$ck-robpercell-$robot_per_cella-numcellocc-$num_celle_occupate-loop-$loop_count-$run"; touch $FILE
					
						N=0
						while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
						do
							echo $BEGIN
							echo $CURRT
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
