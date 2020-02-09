#!/bin/bash

MAX_SKIPPED_LP_list="1000000"
LP_list="1024 512 256 80 60"							#numero di lp
THREAD_list="1 5 10 15 20 25 30 35 40"	#numero di thread
TEST_list="phold"						#test
RUN_list="1"							#lista del numero di run

FAN_OUT_list="1" 						#lista fan out
LOOKAHEAD_list="0"		 				#lookahead
LOOP_COUNT_list="500" 					#loop_count 

CKP_PER_list="50"
PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"
TEST_DURATION="10"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

PSTATE_list="1 2 3 4 5 6 7 8 9 10 11 12 13 14"
HEURISTIC_MODE=8
POWER_LIMIT=65.0
STATIC_PSTATE=1
CORES=40

FOLDER="results/results_phold"
RES_FOLDER="results/plots_phold"
