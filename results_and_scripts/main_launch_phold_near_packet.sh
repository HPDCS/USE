#!/bin/bash

machine="packet"
machine_thread_list=${machine}_thread_list
test_launcher=0 #if 1 test launcher

cd phold_nears
echo "./launcher_phold_nears_retry.sh ../machine_conf/${machine}.conf phold_nears_after_paper1_${machine}.conf 0 ${test_launcher}"
./launcher_phold_nears_retry.sh ../machine_conf/${machine}.conf phold_nears_after_paper1_${machine}.conf 0 ${test_launcher}

cd ..


