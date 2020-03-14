#!/bin/bash

source ./pc_traffic_heuristic_config.sh

mkdir -p ${FOLDER}

for fn in $INPUT_list 
do
	for filtering in $POWERCAP_SAMPLE_FILTERING_list
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
								FILE="${FOLDER}/traffic_psf${filtering}-w${pl}-h${hmode}_maxth${threads}-p${pstate}-lp${lp}_conf${fn}_r${run}.dat"
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
					echo LP$lp F$filtering PL$pl H$hmode $rth $rpo
					done
				done
			done
		done
	done
done
