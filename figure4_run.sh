#!/bin/bash

source thread.conf
MAX_SKIPPED_LP_list="1000000"
LP_list="4096"						#number of lps
TEST_list="pcs"					#test
RUN_list="1 2 3 4 5"				

ENFORCE_LOCALITY_list="0 1" #DISTRIBUTED_FETCH

TA_CHANGE_list="300"
TA_DURATION_list="120"
TA_list="0.48 0.24 0.16 0.12"

LOOKAHEAD_list="0" # 0.01" #"0 0.1 0.01"		#lookahead
WINDOW_list="0.1 0.2 0.4 0.8 1.6 3.2"
CURRENT_BINDING_SIZE="2" # 2 4 8"
EVICTED_BINDING_SIZE="2" # 2 4 8"

CKP_PER_list="20" #"10 50 100"

MAX_RETRY="10"
TEST_DURATION="30"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/pcs_win_explore" #/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

mkdir -p ${FOLDER}

#for max_lp in $MAX_SKIPPED_LP_list
#do
#for ck in $CKP_PER_list
#do
#for pub in $PUB_list
#do
#for epb in $EPB_list
#do
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
					make $test ENFORCE_LOCALITY=$df ENABLE_DYNAMIC_SIZING_FOR_LOC_ENF=0 START_WINDOW=$w TA_CHANGE=$tac TA_DURATION=$tad TA=$tav CURRENT_BINDING_SIZE=$cbs EVICTED_BINDING_SIZE=$ebs
					if [ $df = "0" ]; then
						if [ "$w" = "0.1" ]; then
                                                   echo OK
						   
						else
						   echo skip null test
						   continue
						fi
					fi
					for run in $RUN_list
					do
						for lp in $LP_list
						do
							for th in $MAX_THREADS
							do
								EX1="./${test} $th $lp ${TEST_DURATION}"
								
								FILE1="${FOLDER}/${test}_el_${df}-w_${w}-cbs_${cbs}-ebs_${ebs}-ta_${tav}-tad_${tad}-tac_${tac}_th_${th}-lp_${lp}-run_${run}.dat"
								
								touch ${FILE1}
								#echo $FILE1
								#$EX1 > $FILE1
								
								N=0
								while [[ $(grep -c "Simulation ended" $FILE1) -eq 0 ]]
								do
									echo $BEGIN
									echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
									echo $FILE1
									{ timeout $((TEST_DURATION*2)) $EX1; } &> $FILE1
									if test $N -ge $MAX_RETRY ; then echo break; break; fi
									N=$(( N+1 ))
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
#done
#done
#done
