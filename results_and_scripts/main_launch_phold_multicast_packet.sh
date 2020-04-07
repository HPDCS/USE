#!/bin/bash

machine="packet"
machine_thread_list=${machine}_thread_list
test_launcher=0 #if 1 test launcher

cd phold_multicast
echo "./launcher_phold_multicast_retry.sh ../machine_conf/${machine}.conf phold_multicast_after_paper1_${machine}.conf 0 ${test_launcher}"
./launcher_phold_multicast_retry.sh ../machine_conf/${machine}.conf phold_multicast_after_paper1_${machine}.conf 0 ${test_launcher}

cd ..


