#!/bin/bash

source ./pc_pholdhotspotmixed_heuristic.config


mkdir -p ${FOLDER}

for nhs in $NUM_HOTSPOT_list
do
for pc_wd_sz in $POWERCAP_WINDOW_SIZE_list
do
for pc_obs_per in $POWERCAP_OBSERVATION_PERIOD_list
do
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
	for filtering in $POWERCAP_SAMPLE_FILTERING_list
	do
		for test in $TEST_list
		do
			echo make $test POWERCAP=1 NBC=1 POWERCAP_SAMPLE_FILTERING=${filtering} POWERCAP_OBSERVATION_PERIOD=${pc_obs_per} POWERCAP_WINDOW_SIZE=${pc_wd_sz} MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}  PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 CKP_PERIOD=${ck} PRINT_SCREEN=0 FAN_OUT=5 NUM_HOTSPOT_PHASES=2 HOTSPOTS_PHASE1=0 P_HOTSPOT_PHASE1=0 P_HOTSPOT_PHASE2=1 HOTSPOTS_PHASE2=$nhs
			make $test POWERCAP=1 NBC=1 POWERCAP_SAMPLE_FILTERING=${filtering} POWERCAP_OBSERVATION_PERIOD=${pc_obs_per} POWERCAP_WINDOW_SIZE=${pc_wd_sz} MAX_SKIPPED_LP=${max_lp} REVERSIBLE=0 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}  PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 CKP_PERIOD=${ck} PRINT_SCREEN=0 NUM_HOTSPOT_PHASES=2 HOTSPOTS_PHASE1=0 P_HOTSPOT_PHASE1=0 P_HOTSPOT_PHASE2=1 HOTSPOTS_PHASE2=$nhs
			mv $test ${test}_lf_hi

			for pl in $POWER_LIMIT_list
			do
echo $pl
				for hmode in $HEURISTIC_MODE_list
				do
echo $hmode
					for run in $RUN_list
					do
echo $run
						for lp in $LP_list
						do
							for pstate in $PSTATE_list
							do
								for threads in $CORES
								do
								echo $threads
									./create_config.sh $STARTING_THREADS $pstate $hmode $pl
									EX="sudo ./${test}_lf_hi $threads $lp"
									FILE="${FOLDER}/${test}-lf-dymelor-hijacker-nhs${nhs}-psf${filtering}-w${pl}-h${hmode}-pop${pc_obs_per}-pws${pc_wd_sz}-th${threads}-p${pstate}-${lp}-maxlp-${max_lp}-look-${lookahead}-ck_per-${ck}-fan-${fan_out}-loop-${loop_count}-${run}"; touch $FILE

									N=0
									while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
									do
										echo $BEGIN
										echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
										echo $FILE
										#1> $FILE 2>&1 time $EX
										(time $EX) &> $FILE
										if test $N -ge $MAX_RETRY ; then echo break; break; fi
										N=$(( N+1 ))
									done 
								done
							done
						done
					done
				done
			done
			rm ${test}_lf_hi
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
