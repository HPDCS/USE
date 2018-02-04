#!/bin/bash

LP_list="1024"					#numero di lp
THREAD_list="2 4 8 16 24 32"		#numero di thread
TEST_list="pholdhotspot"		#test
RUN_list="1"				#lista del numero di run

FAN_OUT_list="1"		#lista fan out
LOOKAHEAD_list="0 0.01"		#lookahead
LOOP_COUNT_list="50 150 400 800"	#loop_count

PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"

HS_list="10 32"
PHS_list="0.5 0.75 1"

TEST_DURATION="20"

FOLDER="results/results_phold_hs" #/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

mkdir results
mkdir results/results_phold
mkdir ${FOLDER}

for run in $RUN_list
do
for hs in $HS_list
do
for phs in $PHS_list
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
	for test in $TEST_list 
	do
		make $test NBC=1 	REVERSIBLE=1 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}  PERC_USED_BUCKET=${pub} ELEM_PER_BUCKET=${epb} REPORT=1 DEBUG=0 SPERIMENTAL=1 HOTSPOTS=${hs} P_HOTSPOT=${phs}
		mv $test ${test}_lf_hi
		
#		for run in $RUN_list
#		do
			for lp in $LP_list
				do
					for threads in $THREAD_list
					do
						EX4="./${test}_lf_hi $threads $lp $TEST_DURATION"
						FILE4="${FOLDER}/${test}-lf-dymelor-hijacker-$threads-$lp-look-$lookahead-fan-$fan_out-loop-${loop_count}-hs-$hs-phs-$phs-$run"; touch $FILE4
						
						N=0 
						while [[ $(grep -c "Simulation ended" $FILE4) -eq 0 ]]
						do
							echo $FILE4
							$EX4 > $FILE4 &
last_pid=$!;sleep 30;kill -KILL $last_pid
						        if test $N -ge $MAX_RETRY ; then echo break; break; fi
							N=$(( N+1 ))
						done  
						
						  
					done
				done
		#done
		#rm ${test}_lf_nohi
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
