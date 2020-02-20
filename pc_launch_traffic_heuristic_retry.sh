#!/bin/bash

source ./pc_traffic_heuristic_config.sh

mkdir -p ${FOLDER}

for fn in $INPUT_list 
do
	for filtering in $POWERCAP_SAMPLE_FILTERING_list
	do
    echo cp topology_${fn}.txt topology.txt
    cp model/traffic/small_topology_${fn}.txt topology.txt

    echo make traffic POWERCAP=1 REPORT=1 DEBUG=0 PRINT_SCREEN=0 SIMPLE_TRAFFIC=1 VERBOSE=1 ONGVT_PERIOD=1000 CKP_PERIOD=50 POWERCAP_SAMPLE_FILTERING=$filtering
    make traffic POWERCAP=1 REPORT=1 DEBUG=0 PRINT_SCREEN=0 SIMPLE_TRAFFIC=1 VERBOSE=1 ONGVT_PERIOD=1000 CKP_PERIOD=50 POWERCAP_SAMPLE_FILTERING=$filtering

	  for pl in $POWER_LIMIT_list
		do
      echo ${pl}
			for hmode in $HEURISTIC_MODE_list
			do
        echo ${hmode}

        for run in $RUN_list
        do
          for lp in $LP_list
          do
            for pstate in $PSTATE_list
            do
              for threads in $CORES
              do
                ./create_config.sh ${STARTING_THREADS} ${pstate} ${hmode} ${pl}
                EX="sudo ./traffic $threads $lp $DURATION"
                FILE="${FOLDER}/traffic_psf${filtering}-w${pl}-h${hmode}_maxth${threads}-p${pstate}-lp${lp}_conf${fn}_r${run}.dat"
                touch ${FILE}

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
        done
      done
    done
  done
done