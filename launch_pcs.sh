#!/bin/bash

MAX_SKIPPED_LP_list="1000000"
LP_list="1024 256"						#numero di lp
THREAD_list="1 2 4 8 16 24 32" #"4 8 16 24 32"		#numero di thread
TEST_list="pcs"					#test
RUN_list="1"				#lista del numero di run

DF_list="0 1" #DISTRIBUTED_FETCH

TA_CHANGE_list="75 150 300"
TA_DURATION_list="60 120"
TA_list="0.16 0.32 0.9"

LOOKAHEAD_list="0 0.01" #"0 0.1 0.01"		#lookahead

CKP_PER_list="20 50" #"10 50 100"

MAX_RETRY="10"
TEST_DURATION="20"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/results_pcs" #/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

mkdir -p ${FOLDER}

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
for tav in $TA_list
do
	for tad in $TA_DURATION_list
	do
		for tac in $TA_CHANGE_list
		do
			for test in $TEST_list 
			do
				for df in $DF_list
				do
					make $test DISTRIBUTED_FETCH=$df TA_CHANGE=$tac TA_DURATION=$tad TA=$tav
					
					for run in $RUN_list
					do
						for lp in $LP_list
						do
							for th in $THREAD_list
							do
								EX1="./${test} $th $lp"
								
								FILE1="${FOLDER}/${test}_df_${df}-ta_${tav}-tad_${tad}-tac_${tac}_$th-$lp.dat"
								
								touch ${FILE1}
								#echo $FILE1
								#$EX1 > $FILE1
								
								N=0 
								while [[ $(grep -c "Simulation ended" $FILE1) -eq 0 ]]
								do
									echo $BEGIN
									echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
									echo $FILE1
									$EX1 > $FILE1
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
#done
#done
#done
#done
#done
#done
