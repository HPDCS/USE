#!/bin/bash

COMPILE=0
TA=0.48

if [[ $COMPILE -eq 1 ]]; then
make pcs ENFORCE_LOCALITY=1 TA_CHANGE=300 TA_DURATION=120 TA=${TA} CHANNELS_PER_CELL=1000 CURRENT_BINDING_SIZE=2 EVICTED_BINDING_SIZE=2 PARALLEL_ALLOCATOR=1 MBIND=1 NUM_HOT=400 NUMA_REBALANCE=1 DISTRIBUTED_FETCH=1
mv pcs pcs_hs_lo_re_df

make pcs ENFORCE_LOCALITY=1 TA_CHANGE=300 TA_DURATION=120 TA=${TA} CHANNELS_PER_CELL=1000 CURRENT_BINDING_SIZE=2 EVICTED_BINDING_SIZE=2 PARALLEL_ALLOCATOR=1 MBIND=1 NUM_HOT=400 NUMA_REBALANCE=1
mv pcs pcs_hs_lo_re

make pcs ENFORCE_LOCALITY=1 TA_CHANGE=300 TA_DURATION=120 TA=${TA} CHANNELS_PER_CELL=1000 CURRENT_BINDING_SIZE=2 EVICTED_BINDING_SIZE=2 PARALLEL_ALLOCATOR=1 MBIND=1 NUM_HOT=400 
mv pcs pcs_hs_lo

make pcs TA_CHANGE=300 TA_DURATION=120 TA=${TA} CHANNELS_PER_CELL=1000  NUM_HOT=400 
mv pcs pcs_hs



make pcs ENFORCE_LOCALITY=1 TA_CHANGE=300 TA_DURATION=120 TA=${TA} CHANNELS_PER_CELL=1000 CURRENT_BINDING_SIZE=2 EVICTED_BINDING_SIZE=2 PARALLEL_ALLOCATOR=1 MBIND=1 NUMA_REBALANCE=1 DISTRIBUTED_FETCH=1
mv pcs pcs_lo_re_df

make pcs ENFORCE_LOCALITY=1 TA_CHANGE=300 TA_DURATION=120 TA=${TA} CHANNELS_PER_CELL=1000 CURRENT_BINDING_SIZE=2 EVICTED_BINDING_SIZE=2 PARALLEL_ALLOCATOR=1 MBIND=1 NUMA_REBALANCE=1
mv pcs pcs_lo_re

make pcs ENFORCE_LOCALITY=1 TA_CHANGE=300 TA_DURATION=120 TA=${TA} CHANNELS_PER_CELL=1000 CURRENT_BINDING_SIZE=2 EVICTED_BINDING_SIZE=2 PARALLEL_ALLOCATOR=1 MBIND=1 
mv pcs pcs_lo

make pcs TA_CHANGE=300 TA_DURATION=120 TA=${TA} CHANNELS_PER_CELL=1000  
fi

FOLDER="results_new_obj_$TA"
exe_list="pcs_hs_lo_re_df pcs_hs_lo_re pcs_hs_lo pcs_hs pcs_lo_re_df pcs_lo_re pcs_lo pcs"
exe_list="pcs_hs_lo_re_df pcs_hs_lo_re pcs_hs_lo pcs_hs pcs_lo_re_df pcs_lo_re pcs_lo pcs"

exe_list="pcs_hs_lo_re_df pcs_hs_lo pcs_hs"
exe_list="pcs_lo_re_df pcs_lo pcs"

time_list="30 60 120 240"
lp_list="256 1024 4096"
run_list="0"

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
				filename=$FOLDER/$exe-48-$lp-$time-$r
				EX1="./$exe 48 $lp $time"
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