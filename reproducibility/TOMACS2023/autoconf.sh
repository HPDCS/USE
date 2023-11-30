#!/bin/bash


if [ -f thread.conf ]; then
	source thread.conf
else
MAX_THREADS=$( cat /proc/cpuinfo | grep processor | wc -l)     #maximum number of threads, which is typically equal to the number of (logical) cores.
QUARTER=$(($MAX_THREADS/4))


THREAD_list="1 $((QUARTER/2)) $QUARTER $((QUARTER*2)) $((QUARTER*3)) $MAX_THREADS"    #thread counts to be used for scalability charts



echo "MAX_THREADS=\"${MAX_THREADS}\"" > thread.conf
echo -ne "THREAD_list=\""  >> thread.conf
for i in ${THREAD_list}; do
	echo -ne "$i " >> thread.conf
done
echo "\"" >> thread.conf
fi

BIN_PATH="use-release/test"
