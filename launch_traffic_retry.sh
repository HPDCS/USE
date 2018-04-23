#!/bin/bash

#MAX_SKIPPED_LP_list="1000000"
LP_list="1024"					#numero di lp
THREAD_list="2 4 8 16 24 32" #"4 8 16 24 32"	#numero di thread
TEST_list="traffic"		#test
RUN_list="1"					#lista del numero di run

LOOKAHEAD_list="0 0.01" #"0 0.1 0.01"	#lookahead

#CKP_PER_list="50" #"10 50 100"

#PUB_list="0.33"
#EPB_list="3"

MAX_RETRY="10"

#TEST_DURATION="20"

FOLDER="results/results_traffic" #/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

INPUT_list="30_01 60_01 30_05 60_05"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

mkdir -p ${FOLDER}

#for max_lp in $MAX_SKIPPED_LP_list
#do
#for ck in $CKP_PER_list
#do
#for pub in $PUB_list
#do
#for epb in $EPB_list
#do
#for lookahead in $LOOKAHEAD_list
#do
for fn in $INPUT_list 
do
	for test in $TEST_list 
	do
		echo cp topology_${fn}.txt topology.txt
		cp model/traffic/topology_${fn}.txt model/traffic/topology.txt
				
		echo $test REPORT=1 DEBUG=0 PRINT_SCREEN=0
		make $test REPORT=1 DEBUG=0 PRINT_SCREEN=0
		
		for run in $RUN_list
		do
			for lp in $LP_list
			do
				for threads in $THREAD_list
				do
					EX="./${test} $threads $lp"
					FILE="${FOLDER}/${test}_$threads-$lp_$fn_$run.dat"; touch $FILE
					
					N=0 
					while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
					do
						echo $BEGIN
						echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
						echo $FILE
						$EX > $FILE
						if test $N -ge $MAX_RETRY ; then echo break; break; fi
						N=$(( N+1 ))
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
