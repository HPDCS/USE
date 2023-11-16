#!/bin/bash

source autoconf.sh

RUN="0 1 2 3 4 5"
LPS="4096"
MIN="420"



for r in $RUN; do
for lp in $LPS; do
cmd="use-release/test/test_pcs --nprocesses=$lp --ckpt-autonomic-period --enable-hot --enable-dyn-hot -w $MIN --enable-custom-alloc --enable-mbind"


res="results/pcs-dynhot"

cur="$cmd --ncores=1"
f="$res/seq_hs-1-$lp-$MIN-$r"
echo $cur > $f.sh
./$cur > $f.dat

f="$res/pcs_hs-${MAX_THREADS}-$lp-$MIN-$r"
cmd="$cmd --enforce-locality --el-dyn-window --el-locked-size=2 --el-evicted-size=2"
cur="$cmd --ncores=${MAX_THREADS}"
f="pcs_hs_lo-${MAX_THREADS}-$lp-$MIN-$r"
echo $cur > $f.sh
./$cur > $f.dat

cmd="$cmd --numa-rebalance --distributed-fetch"
cur="$cmd --ncores=${MAX_THREADS}"
f="pcs_hs_lo_re_df-${MAX_THREADS}-$lp-$MIN-$r"
echo $cur > $f.sh
./$cur > $f.dat

done
done