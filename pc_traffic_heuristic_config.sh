#!/bin/bash

MAX_SKIPPED_LP_list="1000000"
LP_list="137"					#number of lps:137 for small topology; 163 for medium tobology
THREAD_list="1 5 10 15 20 25 30 35 40"	#numero di thread
TEST_list="traffic"						#test
RUN_list="0 1 2 3 4"							#lista del numero di run

LOOKAHEAD_list="0"		 				#lookahead

INPUT_list="03_08" # 05_08 10_08 04_08 07_08" #04_08 07_08

CKP_PER_list="50"
PUB_list="0.33"
EPB_list="3"

MAX_RETRY="10"
DURATION="120"

BEGIN="BEGIN TEST:.............$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="CURRENT TEST STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

POWERCAP_SAMPLE_FILTERING_list="0 1 2 3"

PSTATE_list="0"
HEURISTIC_MODE_list="10 11"
POWER_LIMIT_list="55.0 65.0 45.0"
STATIC_PSTATE="1"
CORES="20 40"

FOLDER="results/results_traffic_h"
RES_FOLDER="results/plots_traffic_h"
