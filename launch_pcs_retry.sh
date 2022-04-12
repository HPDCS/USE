#!/bin/bash

MAX_SKIPPED_LP_list="1000000"
LP_list="1024"						#number of lps
THREAD_list="40" #"40"		#number of threads
TEST_list="pcs"					#test list
RUN_list="1"				#run number list

#FAN_OUT_list="1" # 50"				#fan out list
LOOKAHEAD_list="0" # 0.01" #"0 0.1 0.01"		#lookahead list
#LOOP_COUNT_list="50 150 400 800 1600" 				#loop_count #50 100 150 250 400 600" 400=60micsec
CKP_PER_list="50" #"10 50 100"
STATIC_WINDOW_list="0.4"
LOCKED_PIPE_SIZE_list="1"
UNLOCKED_PIPE_SIZE_list="1"

PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"
TEST_DURATION="30"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/pcs_static_win/" #/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

mkdir -p results
mkdir -p $FOLDER

for max_lp in $MAX_SKIPPED_LP_list
do
for ck in $CKP_PER_list
do
for pub in $PUB_list
do
for epb in $EPB_list
do
for w in $STATIC_WINDOW_list
do
for lookahead in $LOOKAHEAD_list
do
	for test in $TEST_list 
	do
                cmd_comp = make $test MAX_SKIPPED_LP=${max_lp} LOOKAHEAD=${lookahead} PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb}PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} CKP_PERIOD=${ck}
		echo $cmd #make $test NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT_US=${loop_count}  PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 CKP_PERIOD=${ck}
		$cmd
		mv $test ${test}_original
                $cmd ENFORCE_LOCALITY=1
                mv $test ${test}_locality
		
		for run in $RUN_list
		do
			for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						EX="./${test}_original $threads $lp $TEST_DURATION"
						FILE="${FOLDER}/${test}-ori-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-$run"; touch $FILE
												
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
						
						EX="./${test}_locality $threads $lp $TEST_DURATION"
                                                FILE="${FOLDER}/${test}-loc-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-$run"; touch $FILE

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
done
