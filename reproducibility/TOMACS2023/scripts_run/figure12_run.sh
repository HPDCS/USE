#!/bin/bash


source autoconf.sh
LP_list="1024"						#number of lps
TEST_list="tuberculosis"					#test list
RUN_list="0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35"				#run number list


MAX_RETRY="10"
ENF_LOCALITY_list="0 1"
NUMA_LIST="0 1"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

TEST_DURATION="60"


CURRENT_BINDING_SIZE="2" 
EVICTED_BINDING_SIZE="2" 

FOLDER="results/results_tuberculosis"
mkdir -p results
mkdir -p results/results_tuberculosis

for numa in $NUMA_LIST
do
for enfl in $ENF_LOCALITY_list
do

	for test in $TEST_list 
	do

		cmd="./${BIN_PATH}/test_$test "
		memory_options="--numa-rebalance --distributed-fetch"
		locality_options="--enforce-locality --enable-custom-alloc --enable-mbind --el-locked-size=${CURRENT_BINDING_SIZE} --el-evicted-size=${EVICTED_BINDING_SIZE} --el-dyn-window"

		if [ $numa == "1" ] && [ $enfl == "0" ]; then
			echo skip this case
			continue
		fi
                

		if [ $enfl == "1" ]; then
			cmd="$cmd ${locality_options}"

			if [ $numa == "1" ]; then
				cmd="$cmd ${memory_options}"
			fi
		fi

		for run in $RUN_list
		do
			for lp in $LP_list
				do
					for threads in "$MAX_THREADS"
					do
						runtime_options="-w ${TEST_DURATION} --ncores=${threads} --nprocesses=$lp --ckpt-autonomic-period  --observe-period=1000 --ckpt-autoperiod-bound=50 --el-th-trigger=0.035 --disable-committer-threads --segment-size-shift=1"
						ecmd="$cmd ${runtime_options}"
						echo $ecmd
							
						EX="./$ecmd"
								
						FILE="${FOLDER}/${test}-enfl_${enfl}-numa_${numa}-threads_${threads}-lp_${lp}-run_${run}"; 

						touch ${FILE}

						N=0 
						while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
						do
							echo $BEGIN
							echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
							echo $FILE
							#1> $FILE 2>&1 time $EX
							{ timeout $((TEST_DURATION*2)) $EX; } &> $FILE
							if test $N -ge $MAX_RETRY ; then 
								echo "" >> $FILE
								echo $ecmd >> $FILE
								echo break; 
								break; 
							fi
							N=$(( N+1 ))
							echo "" >> $FILE
							echo $ecmd >> $FILE
						done  
					done
			done
		 done
	         
      done
done
done

#exit


for test in $TEST_list 
do

	cmd="./${BIN_PATH}/test_$test "

	for run in $RUN_list
	do
		for lp in $LP_list
			do
					runtime_options="-w ${TEST_DURATION} --ncores=1 --nprocesses=$lp --ckpt-autonomic-period   --observe-period=1000"
					ecmd="$cmd ${runtime_options}"
					echo $ecmd
						
					EX="./$ecmd"
							
					FILE="${FOLDER}/${test}-enfl_0-numa_0-threads_1-lp_${lp}-run_${run}"; 

					touch ${FILE}
					
					N=0 
					while [[ $(grep -c "Simulation ended" $FILE) -eq 0 ]]
					do
						echo $BEGIN
						echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
						echo $FILE
						#1> $FILE 2>&1 time $EX
						{ timeout $((TEST_DURATION*2)) $EX; } &> $FILE
						if test $N -ge $MAX_RETRY ; then 
							echo "" >> $FILE
							echo $ecmd >> $FILE
							echo break; 
							break; 
						fi
						N=$(( N+1 ))
						echo "" >> $FILE
						echo $ecmd >> $FILE
					done  

			done
	done
done