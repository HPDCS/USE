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

		cmd="./${BIN_PATH}/test_$test "
		model_options="--fan-out=${fan_out} --event-us=${loop_count}"
		memory_options="--enable-custom-alloc --enable-mbind --numa-rebalance --distributed-fetch"
		locality_options="--enforce-locality --el-locked-size=${CURRENT_BINDING_SIZE} --el-evicted-size=${EVICTED_BINDING_SIZE} --el-dyn-window"

		cmd="$cmd ${model_options}"

		if [ $enfl = "1" ]; then
			cmd="$cmd ${memory_options} ${locality_options}"
		fi

		for run in $RUN_list
		do
			for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						runtime_options="-w ${TEST_DURATION} --ncores=${threads} --nprocesses=$lp --ckpt-autonomic-period"
						ecmd="$cmd ${runtime_options}"
						echo $ecmd
							
						EX="./$ecmd"
								
						FILE="${FOLDER}/${test}-enfl_${enfl}-threads_${threads}-lp_${lp}-maxlp_${max_lp}-look_${lookahead}-ck_per_${ck}-fan_${fan_out}-loop_${loop_count}-run_${run}"; 

						touch ${FILE}

						N=0 
						while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
						do
							echo $BEGIN
							echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
							echo $FILE
							#1> $FILE 2>&1 time $EX
							{ timeout $((TEST_DURATION*2)) $EX; } &> $FILE
							if test $N -ge $MAX_RETRY ; then 
								echo "" >> $FILE
								echo $ecmd >> $FILE
								echo break; 
								break; 
							fi
							N=$(( N+1 ))
							echo "" >> $FILE
							echo $ecmd >> $FILE
						done  
					done
				done
		done
	done
done
done
done
done
done
done
done
done

