#!/bin/bash

source $1
source $2

SIM_list="ori ipi"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

cd ../..
mkdir -p ${DAT_FOLDER}

OUT_SPEEDUP=${DAT_FOLDER}/speedup.txt

for max_lp in $MAX_SKIPPED_LP_list
do
for ck in $CKP_PER_list
do
for pub in $PUB_list
do
for epb in $EPB_list
do
for ta in $TA_list
do
for ta_duration in $TA_DURATION_list
do
for channels_per_cell in $CHANNELS_PER_CELL_list
do
for ta_change in $TA_CHANGE_list
do
for lookahead in $LOOKAHEAD_list
do
	for test in $TEST_list 
	do
		for lp in $LP_list
		do
			line="THREADS "
			for sim in $SIM_list
			do
			 line="$line $sim"
			done
			
			OUT="${DAT_FOLDER}/${test}-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-ta-$ta-ta_duration-$ta_duration-chan_per_cell-$channels_per_cell-ta_change-$ta_change"; 				
			echo $line > $OUT
			
			for threads in $THREAD_list
			do
				line="$threads "
				list_avg=""
				for sim in $SIM_list
				do
					th=0
					count=0
					sum=0
					for run in $RUN_list
					do
						FILE="${SRC_FOLDER}/${test}-$sim-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-ta-$ta-ta_duration-$ta_duration-chan_per_cell-$channels_per_cell-ta_change-$ta_change-$run"; 			
						th=`grep "EventsPerSec"     $FILE | cut -f2 -d':'`
						th=`python2 -c "print '$th'.strip()"`
						sum=`python2 -c "print $th+$sum"`
						count=$(($count+1))
					done
					avg=`python2 -c "print $sum/$count"`
					list_avg="$list_avg $avg "
					line="$line $avg"
				done
				arr=($list_avg)
				speedup=`python2 -c "print ${arr[1]}/${arr[0]}"`
				echo "${test}-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-ta-$ta-ta_duration-$ta_duration-chan_per_cell-$channels_per_cell-ta_change-$ta_change speedup: ${speedup}" >> ${OUT_SPEEDUP}
				echo $line >> $OUT
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


cd scripts_PADS2020/pcs
