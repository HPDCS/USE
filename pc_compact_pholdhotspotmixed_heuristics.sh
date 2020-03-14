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
			for pl in $POWER_LIMIT_list
			do
				for hmode in $HEURISTIC_MODE_list
				do
					for lp in $LP_list
					do
						for pstate in $PSTATE_list
						do
							for threads in $CORES
							do
rth=0
rpo=0
c=0
								for run in $RUN_list
								do
									FILE="${FOLDER}/${test}-lf-dymelor-hijacker-nhs${nhs}-psf${filtering}-w${pl}-h${hmode}-pop${pc_obs_per}-pws${pc_wd_sz}-th${threads}-p${pstate}-${lp}-maxlp-${max_lp}-look-${lookahead}-ck_per-${ck}-fan-${fan_out}-loop-${loop_count}-${run}"; touch $FILE
									th=`grep "EventsPerSec"     $FILE | cut -f2 -d':'`
									po=`grep "Power consumption (net value)"  $FILE | cut -f2 -d':'`
									th=`python -c "print '$th'.strip()"`
									po=`python -c "print '$po'.strip()"`
									rth=`python -c "print $th+$rth"`
									rpo=`python -c "print $po+$rpo"`
									c=$(($c+1))
								done
								rth=`python -c "print $rth/$c"`
								rpo=`python -c "print $rpo/$c"`
							done  
						echo LP$lp NHS$nhs F$filtering PL$pl H$hmode $rth $rpo
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
