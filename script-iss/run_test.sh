#!/bin/bash


TH_list="4"
LP_list="256"
vers="Release"
ckpt_list="10 20 30 40"
duration="30"
RUN_list="1"

respath="results"

mkdir -p ../$vers
cd ../$vers/
echo cmake .. -DCMAKE_BUILD_TYPE=$vers
cmake .. -DCMAKE_BUILD_TYPE=$vers

make -j 4

mkdir -p $respath


for ckpt in $ckpt_list; do
  for lp in $LP_list; do
    for th in $TH_list; do
      for run in $RUN_list; do
        cmd="./test/test_pcs -c $th -p $lp -w $duration --iss_enabled --iss_signal_mprotect --enable-custom-alloc --segment-size-shift=1 --ckpt-period=${ckpt}"
        echo $cmd
        file="$respath/pcs_mprotect_lp_$lp-th_$th_ckpt-${ckpt}.dat"
        echo $file
        $cmd > $file
        touch ${file}
      done
    done
  done
done

for ckpt in $ckpt_list; do
  for lp in $LP_list; do
    for th in $TH_list; do
      for run in $RUN_list; do
        cmd="./test/test_pcs -c $th -p $lp -w $duration --iss_enabled --iss_enabled_mprotection --enable-custom-alloc --segment-size-shift=1 --ckpt-period=${ckpt}"
        echo $cmd
        file="$respath/pcs_lkm_lp_$lp-th_$th_ckpt-${ckpt}.dat"
        echo $file
        $cmd > $file
        touch ${file}
      done
    done
  done
done



