#!/bin/bash

alloc_list="posix_memalign aligned_alloc memalign valloc pvalloc malloc calloc realloc reallocarray alloc align rsfree free"
ignore="glo_alloc thr_alloc node_malloc glo_memalign_alloc thr_memalign_alloc lpm_alloc lpm_memalign_alloc"
dirs="./reverse mm/allocators/common"

lis1=""
for i in $ignore; do
  lis1="$lis1 -e \"$i(\""
done

for i in $dirs; do
  lis1="$lis1 -e \"$i\""
done

#echo $lis1

lis2=""
for a in ${alloc_list}; do
  lis2="$get -e \"$a(\""
done

for i in ${alloc_list}; do
grep -nri -F "$i(" . | grep -v \
-e "glo_calloc(" -e "glo_alloc(" -e "glo_memalign_alloc(" -e "glo_aligned_alloc(" \
-e "thr_calloc(" -e "thr_alloc(" -e "thr_memalign_alloc(" -e "thr_aligned_alloc(" \
-e "lpm_alloc(" -e "lpm_memalign_alloc(" \
-e "lps_alloc(" -e "lps_memalign_alloc(" \
-e "glo_free(" -e "thr_free(" -e "lpm_free(" -e "lps_free(" \
-e "node_malloc(" -e "node_free(" \
-e "do_malloc(" -e "do_free(" -e "find_next_free(" \
-e "./reverse" -e "mm/allocators/" \
-e "__wrap_malloc(" -e "__wrap_realloc("  -e "__wrap_calloc(" -e "__wrap_free" \
-e "buddy_alloc(" -e "buddy_free("
done


