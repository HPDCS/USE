#!/bin/bash


source autoconf.sh
MAX_SKIPPED_LP_list="1000000"
LP_list="1024"						#number of lps
TEST_list="phold"					#test list
RUN_list="1 2 3 4 5"				#run number list

FAN_OUT_list="1" # 50"				#fan out list
LOOKAHEAD_list="0" # 0.01" #"0 0.1 0.01"		#lookahead list
LOOP_COUNT_list="1 5 10 25 50 100 200 400" 				#loop_count #50 100 150 250 400 600" 400=60micsec

CKP_PER_list="20" #"10 50 100"

PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"
TEST_DURATION="20"
ENF_LOCALITY_list="0 1"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

TEST_DURATION="20"


CURRENT_BINDING_SIZE="2" 
EVICTED_BINDING_SIZE="2" 

FOLDER="results/results_phold"
mkdir -p results
mkdir -p results/results_phold

for enfl in $ENF_LOCALITY_list
do
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


		cmd="make test"
		cmd="$cmd ENFORCE_LOCALITY=${enfl} ENABLE_DYNAMIC_SIZING_FOR_LOC_ENF=0  CURRENT_BINDING_SIZE=${CURRENT_BINDING_SIZE} EVICTED_BINDING_SIZE=${EVICTED_BINDING_SIZE}"
		if [ $enfl = "1" ]; then
						cmd="$cmd MBIND=1 NUMA_REBALANCE=1 DISTRIBUTED_FETCH=1"
		fi
		cmd="$cmd PARALLEL_ALLOCATOR=1 MAX_SKIPPED_LP=${max_lp}   PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 CKP_PERIOD=${ck}"
		cmd="$cmd LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT_US=${loop_count}"

		echo $cmd
		$cmd

		for run in $RUN_list
		do
			for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						EX="./${test} $threads $lp ${TEST_DURATION}"
								
						FILE="${FOLDER}/${test}-enfl_${enfl}-threads_${threads}-lp_${lp}-maxlp_${max_lp}-look_${lookahead}-ck_per_${ck}-fan_${fan_out}-loop_${loop_count}-run_${run}"; touch $FILE
												
						N=0 
						while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
						do
							echo $BEGIN
							echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
							echo $FILE
							#1> $FILE 2>&1 time $EX
							{ timeout $((TEST_DURATION*2)) $EX; } &> $FILE
							if test $N -ge $MAX_RETRY ; then echo break; break; fi
							N=$(( N+1 ))
						done  
						echo "" >> $FILE1
						echo $cmd >> $FILE1
						
						  
					done
				done
		done
		rm ${test}
	done
done
done
done
done
done
done
done
done

