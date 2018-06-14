#!/bin/bash

LP_list="1024"					#number of lps
THREAD_list="1 4 8 16 32"		#number of threads
TEST_list="phold"				#test list
RUN_list="1 2"					#list of number of runs

FAN_OUT_list="1 10 25 50"		#fan out list
LOOKAHEAD_list="0.1 0.01 0.0001"		#lookahead
LOOP_COUNT_list="50 100 150"	#loop_count

FOLDER="results/results_phold/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

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
						echo "./${test}_sl_nohi $threads $lp > ${FOLDER}/${test}-sl-dymelor-nohijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run "
							  ./${test}_sl_nohi $threads $lp > ${FOLDER}/${test}-sl-dymelor-nohijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run
						echo "./${test}_lf_nohi $threads $lp > ${FOLDER}/${test}-lf-dymelor-nohijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run"
							  ./${test}_lf_nohi $threads $lp > ${FOLDER}/${test}-lf-dymelor-nohijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run
						echo "./${test}_sl_hi   $threads $lp >   ${FOLDER}/${test}-sl-dymelor-hijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run  "
							  ./${test}_sl_hi   $threads $lp >   ${FOLDER}/${test}-sl-dymelor-hijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run  
						echo "./${test}_lf_hi   $threads $lp >   ${FOLDER}/${test}-lf-dymelor-hijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run  "
							  ./${test}_lf_hi   $threads $lp >   ${FOLDER}/${test}-lf-dymelor-hijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-$loop_count$_run  
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
