#!/bin/bash

COMPILE=1
TA=$1
THREADS=$2

if [[ $COMPILE -eq 1 ]]; then
make pcs ENFORCE_LOCALITY=1 TA_CHANGE=300 TA_DURATION=120 TA=${TA} CHANNELS_PER_CELL=1000 CURRENT_BINDING_SIZE=2 EVICTED_BINDING_SIZE=2 PARALLEL_ALLOCATOR=1 MBIND=1 NUMA_REBALANCE=1 DISTRIBUTED_FETCH=1
mv pcs pcs_hs_lo_re_df

make pcs ENFORCE_LOCALITY=1 TA_CHANGE=300 TA_DURATION=120 TA=${TA} CHANNELS_PER_CELL=1000 CURRENT_BINDING_SIZE=2 EVICTED_BINDING_SIZE=2 
mv pcs pcs_hs_lo

make pcs TA_CHANGE=300 TA_DURATION=120 TA=${TA} CHANNELS_PER_CELL=1000 
mv pcs pcs_hs
fi


FOLDER="results/pcs-$TA"
exe_list="pcs_hs_lo_re_df pcs_hs_lo pcs_hs"

time_list="240"
lp_list="256 1024 4096"
run_list="0 1 2 3 4 5"

MAX_RETRY="2"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"


tot_time=0
mkdir -p $FOLDER
for exe in $exe_list; do
	for time in $time_list; do
		for lp in $lp_list; do
			for r in $run_list; do
				tot_time=$(echo $tot_time + $time | bc)
			done
		done
	done
done
echo $tot_time

for exe in $exe_list; do
	for time in $time_list; do
		for lp in $lp_list; do
			for r in $run_list; do
				filename=$FOLDER/$exe-$THREADS-$lp-$time-$r
				EX1="./$exe $THREADS $lp $time"
				N=0 
				while [[ $(grep -c "Simulation ended" $filename) -eq 0 ]]
				do
					echo $BEGIN
					echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
					echo $filename
					{ timeout $(($time*2)) $EX1; } &> $filename
					if test $N -ge $MAX_RETRY ; then echo break; break; fi
					N=$(( N+1 ))
				done  
			done
		done
	done
done