#!/bin/bash

RUN="0 1 2 3 4 5"
LPS="4096"
MIN="420"

FOLDER="results/pcs-hs-dyn-hot-0.4"

mkdir $FOLDER

for r in $RUN; do
for lp in $LPS; do
cmd="use-release/test/test_pcs --nprocesses=$lp --ckpt-autonomic-period --enable-hot --enable-dyn-hot -w $MIN --enable-custom-alloc --enable-mbind"

cur="$cmd --ncores=1"
f="$FOLDER/seq_hs-1-$lp-$MIN-$r"
./$cur > $f
echo $cur >> $f

cur="$cmd --ncores=40"
f="$FOLDER/pcs_hs-40-$lp-$MIN-$r"
./$cur > $f
echo $cur >> $f

cmd="$cmd --enforce-locality --el-dyn-window --el-locked-size=2 --el-evicted-size=2"

cur="$cmd --ncores=40"
f="$FOLDER/pcs_hs_lo-40-$lp-$MIN-$r"
./$cur > $f
echo $cur >> $f

cmd="$cmd --numa-rebalance --distributed-fetch --df-bound=17"

cur="$cmd --ncores=40"
f="$FOLDER/pcs_hs_lo_re_df-40-$lp-$MIN-$r"
./$cur > $f
echo $cur >> $f

done
done
