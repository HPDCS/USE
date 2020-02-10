#!/bin/bash

source ./pc_traffic_config.sh

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
#	for op in $OP_list
#	do
#		for enp in $ENTERP_list
#		do
#			for lvp in $LEAVEP_list
#			do
				echo cp topology_${fn}.txt topology.txt
				#cp model/traffic/medium_topology_${fn}.txt topology.txt
				cp model/traffic/small_topology_${fn}.txt topology.txt

				echo make traffic POWERCAP=1 REPORT=1 DEBUG=0 PRINT_SCREEN=0 SIMPLE_TRAFFIC=1 VERBOSE=1 ONGVT_PERIOD=1000 CKP_PERIOD=50
				make traffic POWERCAP=1 REPORT=1 DEBUG=0 PRINT_SCREEN=0 SIMPLE_TRAFFIC=1 VERBOSE=1 ONGVT_PERIOD=1000 CKP_PERIOD=50
				
				for lp in $LP_list
				do
					for pstate in $PSTATE_list
					do
						for threads in $(seq $CORES) #$THREAD_list
						do
							./create_config.sh $threads $pstate $HEURISTIC_MODE $POWER_LIMIT
							EX="sudo ./traffic $CORES $lp $DURATION"
							FILE="${FOLDER}/traffic_${threads}-p$pstate-${lp}_ol${op}_${fn}_${run}.dat"
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
			#done
#		done
#	done
done

done
#done
#done
#done
#done
#done
