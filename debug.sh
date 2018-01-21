#!/bin/bash

YEL="\033[0;33m"
NC="\033[0m"

C=0

BEGIN="BEGIN TEST:........$(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"
CURRT="TEST ${C} STARTED AT $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)"

make pholdcount DEBUG=1

while :
do
	C=$(( $C + 1 ))
	tput setab 7; tput setaf 0;
	echo
	echo $BEGIN
	echo $CURRT
	echo
	tput setab 0; tput setaf 7;
	#gdb -ex=r --args ./$1 $2 $3
	gdb --eval-command=run --eval-command=quit -q --args ./pholdcount $1 $2
	#./$1 $2 $3
	sleep .5
done
