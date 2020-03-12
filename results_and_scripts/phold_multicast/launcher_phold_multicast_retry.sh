#!/bin/bash
#usage parameter1:machine.conf parameter2:model.conf parameter3:versioning number,if 0 versioning incremental parameter4:if it is present then test_launcher compile and run program with duration 1 sec
source $1
source $2

VERSIONING=$3
TEST_CONFIGURATION_AND_SCRIPT=$4
SEC_TEST_CONFIGURATION_AND_SCRIPT=1

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

if [ ${VERSIONING} -eq 0 ]
then
	if [ ${TEST_CONFIGURATION_AND_SCRIPT} -eq 1 ]
	then
		VERSION=$VERSIONING
	else
		echo version >> version_${TEST_list}_file.txt

		VERSION=`cat ./version_${TEST_list}_file.txt | grep -c version`
	fi
else
	VERSION=$VERSIONING
fi

cd ../..

PATH_RESULTS=${FOLDER}${VERSION}

if [ ${TEST_CONFIGURATION_AND_SCRIPT} -eq 1 ]
then
	echo TEST_CONFIGURATION_AND_SCRIPT...
else
	mkdir -p ${PATH_RESULTS}
	cp ./results_and_scripts/$TEST_list/$2  ./${PATH_RESULTS}/$2
fi

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
							for thr_prob_normal in $THR_PROB_NORMAL_list
							do
								for test in $TEST_list 
								do
									COMPILATION_ORI="make $test MAX_ALLOCABLE_GIGAS=${MAX_GIGAS} LINEAR_PINNING=${PINNING_IS_LINEAR} THR_PROB_NORMAL=${thr_prob_normal} NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count} PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 CHECKPOINT_PERIOD=${ck} PRINT_SCREEN=0"
									${COMPILATION_ORI}
			
									mv $test ${test}_ori
			
									COMPILATION_IPI="make $test MAX_ALLOCABLE_GIGAS=${MAX_GIGAS} LINEAR_PINNING=${PINNING_IS_LINEAR} THR_PROB_NORMAL=${thr_prob_normal} NBC=1 MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count} PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 CHECKPOINT_PERIOD=${ck} PRINT_SCREEN=0 ENABLE_IPI_MODULE=1"
									${COMPILATION_IPI}
			
									mv $test ${test}_ipi
			
									for run in $RUN_list
									do
										for lp in $LP_list
										do
											for threads in $THREAD_list
											do
												FILE_ORI="${PATH_RESULTS}/${test}-ori-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-fan-$fan_out-loop-$loop_count-prob-${thr_prob_normal}-$run"; touch ${FILE_ORI}
												FILE_IPI="${PATH_RESULTS}/${test}-ipi-$threads-$lp-maxlp-$max_lp-look-$lookahead-ck_per-$ck-fan-$fan_out-loop-$loop_count-prob-${thr_prob_normal}-$run"; touch ${FILE_IPI}
												
												echo ${FILE_ORI}
												echo ${FILE_IPI}

												if [ ${TEST_CONFIGURATION_AND_SCRIPT} -eq 1 ]
												then
													
													./${test}_ori $threads $lp ${SEC_TEST_CONFIGURATION_AND_SCRIPT} "${COMPILATION_ORI}"
													./${test}_ipi $threads $lp ${SEC_TEST_CONFIGURATION_AND_SCRIPT} "${COMPILATION_IPI}"
												else
				
													N=0 
													while [[ $(grep -c "Simulation ended" ${FILE_ORI}) -eq 0 ]]
													do
														echo $BEGIN
														./${test}_ori $threads $lp $TEST_DURATION "${COMPILATION_ORI}" &> ${FILE_ORI}
														
														if test $N -ge $MAX_RETRY ; then echo break; break; fi
														N=$(( N+1 ))
													done  
																
													N=0 
													while [[ $(grep -c "Simulation ended" ${FILE_IPI}) -eq 0 ]]
													do
														echo $BEGIN
														./${test}_ipi $threads $lp $TEST_DURATION "${COMPILATION_IPI}" &> ${FILE_IPI}
														
														if test $N -ge $MAX_RETRY ; then echo break; break; fi
														N=$(( N+1 ))
													done
												fi   
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

cd ./results_and_scripts/$TEST_list
