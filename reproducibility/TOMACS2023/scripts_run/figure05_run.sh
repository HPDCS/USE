#!/bin/bash

source autoconf.sh
MAX_SKIPPED_LP_list="1000000"
LP_list="4096"					#number of lps
TEST_list="pcs"					#test
RUN_list="1 2 3 4 5"				

ENFORCE_LOCALITY_list="0 1" 

TA_CHANGE_list="300"
TA_DURATION_list="120"
TA_list="0.24"

LOOKAHEAD_list="0" 
WINDOW_list="3.2" #"0.1 0.2 0.4 0.8 1.6 3.2"
BASELINE="3.2"
CURRENT_BINDING_SIZE="1 2 4 8" # 16"
EVICTED_BINDING_SIZE="1 2 4 8" # 16"

MAX_RETRY="10"
TEST_DURATION="30"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/pcs_static_win"

mkdir -p ${FOLDER}

for cbs in $CURRENT_BINDING_SIZE
do
for ebs in $EVICTED_BINDING_SIZE
do
for w in $WINDOW_list 
do
for tav in $TA_list
do
	for tad in $TA_DURATION_list
	do
		for tac in $TA_CHANGE_list
		do
			for test in $TEST_list 
			do
				for df in $ENFORCE_LOCALITY_list
				do
					cmd="./${BIN_PATH}/test_$test "
					model_options="--ta=$tav --duration=$tad --handoff-rate=$tac"
					memory_options="--enable-custom-alloc --enable-mbind --numa-rebalance --distributed-fetch"
					locality_options="--enforce-locality --el-locked-size=$cbs --el-evicted-size=$ebs --el-window-size=$w --el-th-trigger-counts=5"

					cmd="$cmd ${model_options}"

					if [ $df = "1" ]; then
						cmd="$cmd ${memory_options} ${locality_options}"
					fi


					if [ $df = "0" ]; then
						if [ "$w" = "$BASELINE" ]; then
                                                   echo OK
						else
						   echo skip null test
						   continue
						fi

						if [ "$ebs" = "1" ]; then echo OK
        	                		else echo skip null test; continue
                	        		fi

						if [ "$cbs" = "1" ]; then echo OK
                       				else echo skip null test; continue
                        			fi
					fi

					for run in $RUN_list
					do
						for lp in $LP_list
						do
							for th in $MAX_THREADS
							do
								runtime_options="-w ${TEST_DURATION} --ncores=$th --nprocesses=$lp --ckpt-autonomic-period"
								cmd="$cmd ${runtime_options}"
								echo $cmd

								EX1="./$cmd"

								FILE1="${FOLDER}/${test}_el_${df}-w_${w}-cbs_${cbs}-ebs_${ebs}-ta_${tav}-tad_${tad}-tac_${tac}_th_${th}-lp_${lp}-run_${run}"
								CMDFILE="$FILE1.sh"
								FILE1="$FILE1.dat"
								echo $cmd > $CMDFILE

								touch ${FILE1}

								N=0
								while [[ $(grep -c "Simulation ended" $FILE1) -eq 0 ]]
								do
									echo $BEGIN
									echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
									echo $FILE1
									{ timeout $((TEST_DURATION*2)) $EX1; } &> $FILE1
									if test $N -ge $MAX_RETRY ; then 
										echo break; 
									break; 
									fi
									N=$(( N+1 ))
									echo "" >> $FILE1
									echo $cmd >> $FILE1
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
