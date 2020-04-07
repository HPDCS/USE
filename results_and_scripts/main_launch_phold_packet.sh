#!/bin/bash

machine="packet"
machine_thread_list=${machine}_thread_list
test_launcher=0

cd phold
echo "./launcher_phold_retry.sh ../machine_conf/${machine_thread_list}.conf phold_after_paper1.conf 0 ${test_launcher}"
./launcher_phold_retry.sh ../machine_conf/${machine_thread_list}.conf phold_after_paper1.conf 0 ${test_launcher}

cd ..

