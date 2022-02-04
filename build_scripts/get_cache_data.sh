#/bin/bash

echo CACHE-ID,CPU,LEVEL,TYPE
for i in /sys/devices/system/cpu/cpu*/cache/index*; do
  type=$(cat $i/type)
  res=$(python3 -c "print(' '.join( '$i/id'.split('/')[5:8] ).replace('cache ', ''))")
  echo $(cat $i/id) $res $type
done

