#!/bin/bash

#usage parameter1:machine.conf parameter2:model.conf parameter3:version to generate/re-run
source $1
source $2

MODEL=pcs

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

VERSION=$3

cd ../..

PATH_RESULT=${FOLDER}${VERSION}
mkdir -p ${PATH_RESULT}

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
for fading_recheck in $FADING_RECHECK_LIST
do
	for test in $TEST_list 
	do
		COMPILATION_ORI="make $test MAX_ALLOCABLE_GIGAS=${MAX_GIGAS} LINEAR_PINNING=${PINNING_IS_LINEAR} NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} TA=${ta} TA_DURATION=${ta_duration} CHANNELS_PER_CELL=${channels_per_cell} TA_CHANGE=${ta_change} FADING_RECHECK_FREQUENCY=${fading_recheck} PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 CHECKPOINT_PERIOD=${ck}"
		echo ${COMPILATION_ORI}
		${COMPILATION_ORI}
		
		mv $test ${test}_ori
		
		COMPILATION_IPI="make $test MAX_ALLOCABLE_GIGAS=${MAX_GIGAS} LINEAR_PINNING=${PINNING_IS_LINEAR} NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} TA=${ta} TA_DURATION=${ta_duration} CHANNELS_PER_CELL=${channels_per_cell} TA_CHANGE=${ta_change} FADING_RECHECK_FREQUENCY=${fading_recheck} PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 CHECKPOINT_PERIOD=${ck} ENABLE_IPI_MODULE=1"
		echo ${COMPILATION_IPI}
			 ${COMPILATION_IPI}
		
		mv $test ${test}_ipi
		
		for run in $RUN_list
		do
			for lp in $LP_list
			do
				for threads in $THREAD_list
				do
					FILE="${PATH_RESULT}/${test}-ori-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-ta-$ta-ta_duration-$ta_duration-chan_per_cell-$channels_per_cell-ta_change-$ta_change-$run"; touch $FILE					
					N=0 
					while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
					do
						echo $BEGIN
						echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
						echo $FILE
						./${test}_ori $threads $lp $TEST_DURATION "${COMPILATION_ORI}" &> $FILE

						if test $N -ge $MAX_RETRY ; then echo break; break; fi
						N=$(( N+1 ))
					done  
					
					FILE="${PATH_RESULT}/${test}-ipi-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-ta-$ta-ta_duration-$ta_duration-chan_per_cell-$channels_per_cell-ta_change-$ta_change-$run"; touch $FILE
											
					N=0 
					while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
					do
						echo $BEGIN
						echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
						echo $FILE
						./${test}_ipi $threads $lp $TEST_DURATION "${COMPILATION_IPI}" &> $FILE
						if test $N -ge $MAX_RETRY ; then echo break; break; fi
						N=$(( N+1 ))
					done  	  
				done
			done
		done
		rm ${test}_ori ${test}_ipi
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
