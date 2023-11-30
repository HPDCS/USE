#!/bin/bash

alloc_list="__thread"
ignore=""
dirs="./variables/"

lis1=""
for i in $ignore; do
  lis1="$lis1 -e \"$i\""
done

for i in $dirs; do
  lis1="$lis1 -e \"$i\""
done

#echo $lis1

for i in ${alloc_list}; do
grep -nri -F "$i" . | grep -v -e "./variables/" -e "./mm/allocators/thr_alloc.h" -e ".sh" -e "extern" -e "./reverse/"
done


