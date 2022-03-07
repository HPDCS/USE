#!/bin/bash

MAX_SKIPPED_LP_list="1000000"
LP_list="256 1024 4096"                      #number of lps
THREAD_list="1 6 12 24 36 48"        #number of  threads
TEST_list="pcs"                 #test
RUN_list="1 2 3 4 5"                

ENFORCE_LOCALITY_list="0 1" #DISTRIBUTED_FETCH

TA_CHANGE_list="300"
TA_DURATION_list="120"
TA_list="0.48 0.24 0.12"

LOOKAHEAD_list="0" # 0.01" #"0 0.1 0.01"        #lookahead
#WINDOW_list="0.1 0.2 0.4 0.8 1.6"
#CURRENT_BINDING_SIZE="1 2 4 8"
#EVICTED_BINDING_SIZE="1 2 4 8"
NCHANNELS_list="1000"



CKP_PER_list="20" #"10 50 100"

MAX_RETRY="10"
TEST_DURATION="30"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

FOLDER="results/pcs_dyn_win_v2" #/results_phold_$(date +%Y%m%d)-$(date +%H%M)"

mkdir -p ${FOLDER}

for nch in $NCHANNELS_list
do
for tav in $TA_list
do
    for tad in $TA_DURATION_list
    do
        for tac in $TA_CHANGE_list
        do
            for test in $TEST_list 
            do
                for df in $ENFORCE_LOCALITY_list
                do
                    make $test ENFORCE_LOCALITY=$df TA_CHANGE=$tac TA_DURATION=$tad TA=$tav CHANNELS_PER_CELL=$nch
                    for run in $RUN_list
                    do
                        for lp in $LP_list
                        do
                            for th in $THREAD_list
                            do
                                EX1="./${test} $th $lp ${TEST_DURATION}"
                                
                                FILE1="${FOLDER}/${test}_el_${df}-nch_${nch}-ta_${tav}-tad_${tad}-tac_${tac}-th_${th}-lp_${lp}-run_${run}.dat"
                                
                                touch ${FILE1}
                                #echo $FILE1
                                #$EX1 > $FILE1
                                
                                N=0 
                                while [[ $(grep -c "Simulation ended" $FILE1) -eq 0 ]]
                                do
                                    echo $BEGIN
                                    echo "CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
                                    echo $FILE1
                                    $EX1 > $FILE1
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
done
