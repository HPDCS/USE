#!/bin/bash

gcc compute_clock.c -o compute_clock
#echo -ne "wainting compute clock..."
clocks=$(./compute_clock 5 | grep us | cut -d':' -f2)
#echo "DONE"

out="clock_constant.h"

echo "#ifndef __CLOCKS_PER_US_H__"     > $out
echo "#define __CLOCKS_PER_US_H__"    >> $out
echo ""                               >> $out
echo "#define CLOCKS_PER_US ((unsigned long long)$clocks)"  >> $out
echo "#endif"                         >> $out


rm compute_clock
