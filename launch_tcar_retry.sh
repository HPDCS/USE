#!/bin/bash

LP_list="1024"				#numero di lp
THREAD_list="1 4 8 16 32"	#numero di thread
TEST_list="tcar"			#test
RUN_list="1 2"				#lista del numero di run

LOOKAHEAD_list="0.2" # 0.002"	#lookahead
LOOP_COUNT_list="4500"		#loop_count
ROB_PER_CELLA_list="2"		#robot per cella
NUM_CELLE_OCC="80 160"		#numero di celle occupate

MAX_RETRY="10"

FOLDER="results/results_tcar2" #/results_tcar_$(date +%Y%m%d)-$(date +%H%M)"

mkdir results
mkdir results/results_tcar
mkdir ${FOLDER}

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
		make $test 			LOOKAHEAD=${lookahead} NUM_CELLE_OCCUPATE=${num_celle_occupate} ROBOT_PER_CELLA=${robot_per_cella} LOOP_COUNT=${loop_count}
		mv $test ${test}_sl_nohi
		
		make $test NBC=1 	LOOKAHEAD=${lookahead} NUM_CELLE_OCCUPATE=${num_celle_occupate} ROBOT_PER_CELLA=${robot_per_cella} LOOP_COUNT=${loop_count}
		mv $test ${test}_lf_nohi
		
		make $test 			REVERSIBLE=1 LOOKAHEAD=${lookahead} NUM_CELLE_OCCUPATE=${num_celle_occupate} ROBOT_PER_CELLA=${robot_per_cella} LOOP_COUNT=${loop_count}
		mv $test ${test}_sl_hi
							
		make $test NBC=1 	REVERSIBLE=1 LOOKAHEAD=${lookahead} NUM_CELLE_OCCUPATE=${num_celle_occupate} ROBOT_PER_CELLA=${robot_per_cella} LOOP_COUNT=${loop_count}
		mv $test ${test}_lf_hi
		for run in $RUN_list 
		do
			for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						EX1="./${test}_sl_nohi $threads $lp"
						EX2="./${test}_lf_nohi $threads $lp"
						EX3="./${test}_sl_hi $threads $lp"
						EX4="./${test}_lf_hi $threads $lp"
						FILE1="${FOLDER}/${test}-sl-dymelor-nohijacker-$threads-$lp-look-$lookahead-robpercell-$robot_per_cella-numcellocc-$num_celle_occupate-loop-$loop_count_$run"; touch $FILE1
						FILE2="${FOLDER}/${test}-lf-dymelor-nohijacker-$threads-$lp-look-$lookahead-robpercell-$robot_per_cella-numcellocc-$num_celle_occupate-loop-$loop_count_$run"; touch $FILE2
						FILE3="${FOLDER}/${test}-sl-dymelor-hijacker-$threads-$lp-look-$lookahead-robpercell-$robot_per_cella-numcellocc-$num_celle_occupate-loop-$loop_count_$run"; touch $FILE3
						FILE4="${FOLDER}/${test}-lf-dymelor-hijacker-$threads-$lp-look-$lookahead-robpercell-$robot_per_cella-numcellocc-$num_celle_occupate-loop-$loop_count_$run"; touch $FILE4
						
						#N=0
						#while [[ $(grep -c "Simulation ended" $FILE1) -eq 0 ]]
						#do
						#	echo $FILE1
						#	$EX1 > $FILE1
						#	if test $N -ge $MAX_RETRY ; then echo break; break; fi
						#	N=$(( N+1 ))
						#done
						# 
						#N=0
						#while [[ $(grep -c "Simulation ended" $FILE2) -eq 0 ]]
						#do
						#	echo $FILE2
						#	$EX2 > $FILE2
						#	if test $N -ge $MAX_RETRY ; then echo break; break; fi
						#	N=$(( N+1 ))
						#done
						 
						N=0
						while [[ $(grep -c "Simulation ended" $FILE3) -eq 0 ]]
						do
							echo $FILE3
							$EX3 > $FILE3
							if test $N -ge $MAX_RETRY ; then echo break; break; fi
							N=$(( N+1 ))
						done 
						 
						N=0
						while [[ $(grep -c "Simulation ended" $FILE4) -eq 0 ]]
						do
							echo $FILE4
							$EX4 > $FILE4
							if test $N -ge $MAX_RETRY ; then echo break; break; fi
							N=$(( N+1 ))
						done  
							 
					done
				done
		done
		rm ${test}_sl_nohi
		rm ${test}_lf_nohi
		rm ${test}_sl_hi
		rm ${test}_lf_hi
	done
	
done
done
done
done
