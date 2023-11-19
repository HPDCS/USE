#!/bin/bash

out="cache.db"


if [[ -f $out ]]; then
  echo $out is already present
else
echo CACHE-ID,CPU,LEVEL,TYPE > $out
for i in /sys/devices/system/cpu/cpu*/cache/index*; do
  type=$(cat $i/type)
  res=$(python3 -c "print(' '.join( '$i/id'.split('/')[5:8] ).replace('cache ', ''))")
  echo $(cat $i/id) $res $type >> $out
done
fi

#python3 build_cache_map.py cache.db > hpipe.h
#mkdir -p ../include-gen
#mv hpipe.h ../include-gen

