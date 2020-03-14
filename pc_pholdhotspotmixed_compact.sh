#!/bin/bash

source ./pc_pholdhotspotmixed.config


mkdir -p ${FOLDER}
mkdir -p ${RES_FOLDER}



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
			for pl in $POWER_LIMIT_list
			do
				for hmode in $HEURISTIC_MODE_list
				do
					for lp in $LP_list 
					do
						FILE1="${RES_FOLDER}/TH-${test}-lf-dymelor-hijacker-nhs${nhs}-psf${filtering}-w${pl}-h${hmode}-pop${pc_obs_per}-pws${pc_wd_sz}-${lp}-maxlp-${max_lp}-look-${lookahead}-ck_per-${ck}-fan-${fan_out}-loop-${loop_count}"; touch $FILE1
						FILE2="${RES_FOLDER}/PO-${test}-lf-dymelor-hijacker-nhs${nhs}-psf${filtering}-w${pl}-h${hmode}-pop${pc_obs_per}-pws${pc_wd_sz}-${lp}-maxlp-${max_lp}-look-${lookahead}-ck_per-${ck}-fan-${fan_out}-loop-${loop_count}"; touch $FILE1
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
						#echo $line1
						#echo $line2
						for threads in $(seq $CORES)
						do
							line1="$threads "
							line2="$threads "
							for pstate in $PSTATE_list
							do
								for run in $RUN_list
								do
									FILE="${FOLDER}/${test}-lf-dymelor-hijacker-nhs${nhs}-psf${filtering}-w${pl}-h${hmode}-pop${pc_obs_per}-pws${pc_wd_sz}-th${threads}-p${pstate}-${lp}-maxlp-${max_lp}-look-${lookahead}-ck_per-${ck}-fan-${fan_out}-loop-${loop_count}-${run}"; touch $FILE
									th=`grep "EventsPerSec"     $FILE | cut -f2 -d':'`
									po=`grep "Power consumption (net value)"  $FILE | cut -f2 -d':'`
									th=`python -c "print '$th'.strip()"`
									po=`python -c "print '$po'.strip()"`
									#echo TH $th PO $po
								done
							line1="$line1 $th"
							line2="$line2 $po"
							done				
							echo $line1 >> $FILE1
							echo $line2 >> $FILE2
							#echo $line1
							#echo $line2
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
done
done
done

done 
