#!/bin/bash

LP_list="1024"						#number of lps
THREAD_list="2 4 8 16 24 32" #"4 8 16 24 32"			#number of threads
TEST_list="tcar"					#test list
RUN_list="1"				#run number list

LOOP_COUNT_list="0" #4500 1000"	#loop_count
LOOKAHEAD_list="0"			#lookahead
ROB_PER_CELLA_list="2 4"				#robot per cella
NUM_CELLE_OCC="80 160 240 320"				#occupied cell list

CKP_PER_list="50" #"10 50 100"

PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"

TEST_DURATION="20"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/results_tcar" #/results_tcar_$(date +%Y%m%d)-$(date +%H%M)"

mkdir -p results
mkdir -p results/results_tcar
#mkdir ${FOLDER}

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
		make $test NBC=1 REVERSIBLE=0 LOOKAHEAD=${lookahead} NUM_CELLE_OCCUPATE=${num_celle_occupate} ROBOT_PER_CELLA=${robot_per_cella} LOOP_COUNT=${loop_count} PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 CKP_PERIOD=${ck} PRINT_SCREEN=0
		mv $test ${test}_lf_hi
		for run in $RUN_list
		do
			for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						EX="./${test}_lf_hi $threads $lp $TEST_DURATION"
						FILE="${FOLDER}/${test}-lf-dymelor-hijacker-$threads-$lp-look-$lookahead-ck_per-$ck-robpercell-$robot_per_cella-numcellocc-$num_celle_occupate-loop-$loop_count-$run"; touch $FILE
					
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
