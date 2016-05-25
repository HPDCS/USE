#!/bin/bash

LP_list="1024"					#numero di lp
THREAD_list="1 4 8 16 32"		#numero di thread
TEST_list="phold"				#test
RUN_list="1 2 3 4"				#lista del 
numero di run

FAN_OUT_list="1 10 25 50"		#lista fan out
LOOKAHEAD_list="0.1 0.001"		#lookahead
LOOP_COUNT_list="600 400 250 150 100 50"	#loop_count

MAX_RETRY="5"

FOLDER="results/results_phold" #/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

mkdir results
mkdir results/results_phold
mkdir ${FOLDER}

for loop_count in $LOOP_COUNT_list
do
for fan_out in $FAN_OUT_list
do
for lookahead in $LOOKAHEAD_list
do
	for test in $TEST_list 
	do
		make $test 			LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}
		mv $test ${test}_sl_nohi
		
		make $test NBC=1 	LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}
		mv $test ${test}_lf_nohi
		
		make $test 			REVERSIBLE=1 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}
		mv $test ${test}_sl_hi
							
		make $test NBC=1 	REVERSIBLE=1 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}
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
						FILE1="${FOLDER}/${test}-sl-dymelor-nohijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run"; touch $FILE1
						FILE2="${FOLDER}/${test}-lf-dymelor-nohijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run"; touch $FILE2
						FILE3="${FOLDER}/${test}-sl-dymelor-hijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run"; touch $FILE3
						FILE4="${FOLDER}/${test}-lf-dymelor-hijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run"; touch $FILE4
						
						N=0
						while [[ $(grep -c "Simulation ended" $FILE1) -eq 0 ]]
						do
							echo $FILE1
							$EX1 > $FILE1
							if test $N -ge $MAX_RETRY ; then echo break; break; fi
							N=$(( N+1 ))
						done
						
						N=0 
						while [[ $(grep -c "Simulation ended" $FILE2) -eq 0 ]]
						do
							echo $FILE2
							$EX2 > $FILE2
							if test $N -ge $MAX_RETRY ; then echo break; break; fi
							N=$(( N+1 ))
						done
						
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
