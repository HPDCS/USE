#!/bin/bash

BEGIN="BEGIN TEST: $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
C=0

#make $1 DEBUG=1

while :
do
	C=$(( $C + 1 ))

	echo $BEGIN
	echo
	echo "TEST ${C} STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
	#gdb -ex=r --args ./$1 $2 $3
	gdb --eval-command=run --eval-command=quit -q --args ./pholdcount $1 $2
	#./$1 $2 $3
	sleep .5
done
