#!/bin/bash

machine="packet"
machine_thread_list=${machine}_thread_list
test_launcher=0

cd pcs_new
echo "./launcher_pcs_new_retry.sh ../machine_conf/${machine}.conf pcs_new_after_paper1_${machine}.conf 0 ${test_launcher}"
./launcher_pcs_new_retry.sh ../machine_conf/${machine}.conf pcs_new_after_paper1_${machine}.conf 0 ${test_launcher}

cd ..

#cd phold
#./launcher_phold_retry.sh ../machine_conf/${machine_thread_list}.conf phold_after_paper1.conf 0 ${test_launcher}

#cd ..

