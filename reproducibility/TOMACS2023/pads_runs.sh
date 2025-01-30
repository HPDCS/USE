#!/bin/bash

source autoconf.sh

echo PCS STARTED

TA_LIST="0.1 0.2 0.8"
TH_LIST="8 16 24 32 40"
for j in ${TA_LIST}; do
for i in ${TH_LIST}; do
./scripts_run/use_pads2025_pcs.sh  $j $i 4096
done
done

echo PCS COMPLETED
echo HIGH STARTED

SCALING="1.5 1.0 0.25"
TH_LIST="8 16 24 32 40"
for j in ${SCALING}; do
for i in ${TH_LIST}; do
./scripts_run/use_pads2025_highway.sh  $j $i 4096
done
done

echo HIGH COMPLETED

