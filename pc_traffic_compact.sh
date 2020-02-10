#!/bin/bash

source ./pc_traffic_config.sh

mkdir -p ${FOLDER}
mkdir -p ${RES_FOLDER}



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
#	for op in $OP_list
#	do
#		for enp in $ENTERP_list
#		do
#			for lvp in $LEAVEP_list
#			do

				for lp in $LP_list
				do
				  FILE1="${RES_FOLDER}/TH-traffic_${lp}_${fn}"; touch $FILE1
			    FILE2="${RES_FOLDER}/PO-traffic_${lp}_${fn}"; touch $FILE2

				  line1="THREADS "
			    line2="THREADS "
			    echo $FILE1

					for pstate in $PSTATE_list
					do
    				line1="$line1 P$pstate"
    				line2="$line2 P$pstate"
    			done
			    echo $line1 > $FILE1
			    echo $line2 > $FILE2

						for threads in $(seq $CORES) #$THREAD_list
						do
						  line1="$threads "
				      line2="$threads "
				      for pstate in $PSTATE_list
				      do
					      for run in $RUN_list
					      do
						      FILE="${FOLDER}/traffic_${threads}-p$pstate-${lp}_ol${op}_${fn}_${run}.dat"
							    th=`grep "EventsPerSec"     $FILE | cut -f2 -d':'`
						      po=`grep "Power consumption (net value)"  $FILE | cut -f2 -d':'`
						      th=`python -c "print '$th'.strip()"`
						      po=`python -c "print '$po'.strip()"`
					      done
				      line1="$line1 $th"
				      line2="$line2 $po"
				      done
				      echo $line1 >> $FILE1
				      echo $line2 >> $FILE2

						done
					done
				done
			#done
#		done
#	done

#done
#done
#done
#done
#done
