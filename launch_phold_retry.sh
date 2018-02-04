#!/bin/bash

LP_list="1024"					#numero di lp
THREAD_list="2 4 8 16 24 32"		#numero di thread
TEST_list="phold"				#test
RUN_list="1"				#lista del numero di run

FAN_OUT_list="1"		#lista fan out
LOOKAHEAD_list="0 0.01"		#lookahead
LOOP_COUNT_list="50 150 400 800 1600" #50 100 150 250 400 600"	#loop_count

PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"

TEST_DURATION="20"

FOLDER="results/results_phold" #/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

mkdir results
mkdir results/results_phold
mkdir ${FOLDER}

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
		make $test NBC=1 	LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}  PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1
		mv $test ${test}_lf_nohi
		
		make $test NBC=1 	REVERSIBLE=1 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}  PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1
		mv $test ${test}_lf_hi
		
		for run in $RUN_list
		do
			for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						#EX2="./${test}_lf_nohi $threads $lp"
						EX4="./${test}_lf_hi $threads $lp $TEST_DURATION" 
						#FILE2="${FOLDER}/${test}-lf-dymelor-nohijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count-$run"; touch $FILE2
						FILE4="${FOLDER}/${test}-lf-dymelor-hijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count-$run"; touch $FILE4
						
						
					#	N=0 
					#	while [[ $(grep -c "Simulation ended" $FILE2) -eq 0 ]]
					#	do
					#		echo $FILE2
					#		$EX2 > $FILE2
					#		if test $N -ge $MAX_RETRY ; then echo break; break; fi
					#		N=$(( N+1 ))
					#	done
						
						N=0 
						while [[ $(grep -c "Simulation ended" $FILE4) -eq 0 ]]
						do
							echo $FILE4
							$EX4 &> $FILE4
							if test $N -ge $MAX_RETRY ; then echo break; break; fi
							N=$(( N+1 ))
						done  
						
						  
					done
				done
		done
		#rm ${test}_lf_nohi
		rm ${test}_lf_hi
	done
done
done
done
done
done
