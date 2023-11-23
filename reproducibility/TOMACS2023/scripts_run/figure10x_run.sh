#!/bin/bash

COMPILE=1
TA=$1
THREADS=$2

model_configuration="--ta=${TA} --duration=120 --handoff-rate=300 --ch=1000 --ckpt-autonomic-period"
memory_options="--numa-rebalance --distributed-fetch"
locality_options="--enable-custom-alloc --enable-mbind --enforce-locality --el-locked-size=2 --el-evicted-size=2 --el-dyn-window --el-th-trigger-counts=5" 


FOLDER="results/pcs-hs-$TA"
exe_list="pcs_hs_lo_re_df pcs_hs_lo pcs_hs"

time_list="240"
lp_list="4096"
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
			runtime_options="--ncores=$THREADS --nprocesses=$lp -w $time  --enable-custom-alloc"
			filename=$FOLDER/$exe-$THREADS-$lp-$time-$r
            cmdfile="$filename.sh"
            filename="$filename.dat"
			EX1="./use-release/test/test_pcs ${runtime_options} ${model_configuration}"
			if [[ $exe != "pcs_hs" ]]; then
				EX1="$EX1 ${locality_options}"
			fi
			if [[ $exe == "pcs_hs_lo_re_df" ]]; then
				EX1="$EX1 ${memory_options}"
			fi
			echo $EX1 > $cmdfile
            N=0 
			while [[ $(grep -c "Simulation ended" $filename) -eq 0 ]]
			do
				echo $BEGIN
				echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
				echo $filename
				echo $EX1
				#break
				{ timeout $(($time*2)) $EX1; } &> $filename
				if test $N -ge $MAX_RETRY ; then echo break; break; fi
				N=$(( N+1 ))
			done  
			echo $EX1 >> $filename
			#echo $f2 >> $filename
			#echo $f3 >> $filename
		done
	done
done
done


for time in $time_list; do
	for lp in $lp_list; do
		for r in $run_list; do
			runtime_options="--ncores=1 --nprocesses=$lp -w $time"
			filename=$FOLDER/seq_hs-1-$lp-$time-$r
			EX1="./use-release/test/test_pcs ${runtime_options} ${model_configuration}"
            cmdfile="$filename.sh"
            filename="$filename.dat"
			echo $EX1 > $cmdfile
			N=0 
			while [[ $(grep -c "Simulation ended" $filename) -eq 0 ]]
			do
				echo $BEGIN
				echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
				echo $filename
				echo $EX1
				#break
				{ timeout $(($time*2)) $EX1; } &> $filename
				if test $N -ge $MAX_RETRY ; then echo break; break; fi
				N=$(( N+1 ))
			done  
			echo $EX1 >> $filename
			#echo $f2 >> $filename
			#echo $f3 >> $filename
		done
	done
done
