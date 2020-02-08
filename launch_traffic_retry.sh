#!/bin/bash

#MAX_SKIPPED_LP_list="1000000"
LP_list="137"					#number of lps:137 for small topology; 163 for medium tobology
THREAD_list="32 16 8 4 2" #"4 8 16 24 32"	#number of threads
RUN_list="1 2 3"					#run number list

#LOOKAHEAD_list="0 0.01" #"0 0.1 0.01"	#lookahead

#OP_list="1 2" #2

#CKP_PER_list="50" #"10 50 100"

#PUB_list="0.33"
#EPB_list="3"

MAX_RETRY="10"

#TEST_DURATION="20"

FOLDER="results/results_traffic" #/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

INPUT_list="03_08"# 05_08 10_08 04_08 07_08" #04_08 07_08
ENTERP_list="30 60"
LEAVEP_list="01 05"


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
for run in $RUN_list
do

for fn in $INPUT_list 
do
	for op in $OP_list
	do
#		for enp in $ENTERP_list
#		do
#			for lvp in $LEAVEP_list
#			do
				echo cp topology_${fn}.txt topology.txt
				#cp model/traffic/medium_topology_${fn}.txt topology.txt
				cp model/traffic/small_topology_${fn}.txt topology.txt

				echo make traffic REPORT=1 DEBUG=0 PRINT_SCREEN=0 SIMPLE_TRAFFIC=1 VERBOSE=1 ONGVT_PERIOD=1000 CKP_PERIOD=50
				make traffic REPORT=1 DEBUG=0 PRINT_SCREEN=0 SIMPLE_TRAFFIC=1 VERBOSE=1 ONGVT_PERIOD=1000 CKP_PERIOD=50
				
				for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						EX="./traffic $threads $lp"
						FILE="${FOLDER}/traffic_${threads}-${lp}_ol${op}_${fn}_${run}.dat"
						touch $FILE
						
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
#		done
#	done
done

done
#done
#done
#done
#done
#done
