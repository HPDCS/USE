#!/bin/bash

MODEL=$1
THREAD=$2
LP=$3
SEC=$4
CHECKPOINT_PERIOD=$5
VERSION=$6

OUTPUT_FILE="results/results_${MODEL}${VERSION}.txt"

PROB_LIST="0 0.25 0.5 0.75 1.0" 	#probabilità di andare sull'hotspot

for prob in $PROB_LIST
do
echo "make ${MODEL} CHECKPOINT_PERIOD=${CHECKPOINT_PERIOD} THR_PROB_NORMAL=${prob}" >> ${OUTPUT_FILE}
make ${MODEL} CHECKPOINT_PERIOD=${CHECKPOINT_PERIOD} THR_PROB_NORMAL=${prob}
echo "test1_${prob}"
./${MODEL} ${THREAD} ${LP} ${SEC} >> ${OUTPUT_FILE}
echo "test2_${prob}"
./${MODEL} ${THREAD} ${LP} ${SEC} >> ${OUTPUT_FILE}

done 

for prob in $PROB_LIST
do
echo "make ${MODEL} POSTING=1 IPI_SUPPORT=1 CHECKPOINT_PERIOD=${CHECKPOINT_PERIOD} THR_PROB_NORMAL=${prob}" >> ${OUTPUT_FILE}
make ${MODEL} POSTING=1 IPI_SUPPORT=1 CHECKPOINT_PERIOD=${CHECKPOINT_PERIOD} THR_PROB_NORMAL=${prob}
echo "test3_${prob}"
./${MODEL} ${THREAD} ${LP} ${SEC} >> ${OUTPUT_FILE}
echo "test4_${prob}"
./${MODEL} ${THREAD} ${LP} ${SEC} >> ${OUTPUT_FILE}

done