#!/bin/bash

echo 1 > /proc/sys/kernel/sysrq

echo 1 > /proc/sys/kernel/panic

echo 1 > /proc/sys/kernel/hung_task_panic          # panic when hung task is detected
echo 1 > /proc/sys/kernel/panic_on_io_nmi          # panic on NMIs from I/O
echo 1 > /proc/sys/kernel/panic_on_oops            # panic on oops or kernel bug detection
echo 1 > /proc/sys/kernel/panic_on_unrecovered_nmi # panic on NMIs from memory or unknown
echo 1 > /proc/sys/kernel/softlockup_panic         # panic when soft lockups are detected
echo 1 > /proc/sys/vm/panic_on_oom                 # panic when out-of-memory happens



echo "end stuff"
