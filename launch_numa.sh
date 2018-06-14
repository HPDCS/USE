#!/bin/bash

MAX_SKIPPED_LP_list="1000000"
LP_list="1024"						#number of lps
THREAD_list="1 2 4 8 16 24 32" #"4 8 16 24 32"		#number of threads
TEST_list="pcs"					#test
RUN_list="1"				

FAN_OUT_list="1" # 50"				#fan out list
LOOKAHEAD_list="0 0.01" #"0 0.1 0.01"		#lookahead
LOOP_COUNT_list="50 150 400 800 1600" 				#loop_count #50 100 150 250 400 600" 400=60micsec

CKP_PER_list="50" #"10 50 100"

PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"
TEST_DURATION="20"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/results_numa" #/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

mkdir -p results
mkdir -p results/results_numa
#mkdir ${FOLDER}

#for max_lp in $MAX_SKIPPED_LP_list
#do
#for ck in $CKP_PER_list
#do
#for pub in $PUB_list
#do
#for epb in $EPB_list
#do
#for loop_count in $LOOP_COUNT_list
#do
#for fan_out in $FAN_OUT_list
#do
#for lookahead in $LOOKAHEAD_list
#do
	for test in $TEST_list 
	do
		make $test 
		mv $test ${test}_base
		
		make $test PARALLEL_ALLOCATOR=1
		mv $test ${test}_pa
		
		make $test PARALLEL_ALLOCATOR=1 MBIND=1
		mv $test ${test}_pa_mb
		
		make $test PARALLEL_ALLOCATOR=1 MBIND=1 DISTRIBUTED_FETCH=1
		mv $test ${test}_pa_mb_df
		
		make $test DISTRIBUTED_FETCH=1 
		mv $test ${test}_df
		
		make $test PARALLEL_ALLOCATOR=1 DISTRIBUTED_FETCH=1 
		mv $test ${test}_pa_df
		
		for run in $RUN_list
		do
			for lp in $LP_list
				do
					for th in $THREAD_list
					do
						EX1="./${test}_base $th $lp"
						EX2="./${test}_pa $th $lp" #_mbind
						EX3="./${test}_pa_mb $th $lp" #_mbind
						EX4="./${test}_pa_mb_df $th $lp" #_mbind
						EX5="./${test}_df $th $lp"
						EX6="./${test}_pa_df $th $lp"
						
						FILE1="${FOLDER}/${test}_$th-$lp"
						FILE2="${FOLDER}/${test}_pa_$th-$lp"
						FILE3="${FOLDER}/${test}_pa_mb_$th-$lp"
						FILE4="${FOLDER}/${test}_pa_mb_df_$th-$lp"
						FILE5="${FOLDER}/${test}_df_$th-$lp"
						FILE6="${FOLDER}/${test}_pa_df_$th-$lp"
						
						touch ${FILE1}
						echo $FILE1
						$EX1 > $FILE1
                    
						touch ${FILE2}
						echo $FILE2
						$EX2 > $FILE2
						
						touch ${FILE3}
						echo $FILE3
						$EX3 > $FILE3
			        
						touch ${FILE4}
						echo $FILE4
						$EX4 > $FILE4

						touch ${FILE5}
						echo ${FILE5}
						$EX5 > $FILE5
						
						touch ${FILE6}
						echo ${FILE6}
						$EX5 > $FILE6
						
						#N=0 
						#while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
						#do
						#	echo $BEGIN
						#	echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
						#	echo $FILE
						#	#1> $FILE 2>&1 time $EX
						#	(time $EX) &> $FILE
						#	if test $N -ge $MAX_RETRY ; then echo break; break; fi
						#	N=$(( N+1 ))
						#done  
						
						  
					done
				done
		done
		#rm ${test}_*
	done
#done
#done
#done
#done
#done
#done
#done
