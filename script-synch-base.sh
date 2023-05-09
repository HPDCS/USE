#!/bin/bash


TH_list="8 16"
LP_list="64"
vers="Debug"
CSR_list="0 1"
ALG_list="1"
DURATION="30"
RUN_list="1"

respath="results-fakecsr"

mkdir -p $respath


for v in $vers; do
cd $v
  for csr in $CSR_list; do
  for alg in $ALG_list; do
    echo cmake .. -DCMAKE_BUILD_TYPE=$v -DCSR_CONTEXT=$csr -DSTATE_SWAPPING=1 
    cmake .. -DCMAKE_BUILD_TYPE=$v -DCSR_CONTEXT=$csr -DSTATE_SWAPPING=1 > /dev/null
    for lp in $LP_list; do
      for th in $TH_list; do
        for run in $RUN_list; do
          make > /dev/null && make remove_modules > /dev/null && make mount_modules > /dev/null 2&>1
          echo ./test/test_pcs $th $lp $duration $respath/pcs-alg_$alg-csr_$csr-lp_$lp-th_$th-run_$run.dat 
        done
      done
    done
  done
  done
cd ..  
done
