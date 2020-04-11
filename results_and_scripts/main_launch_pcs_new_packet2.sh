#!/bin/bash

machine="packet"
machine_thread_list=${machine}_thread_list
test_launcher=1

cd pcs_new
echo "./launcher_pcs_new_retry.sh ../machine_conf/${machine}.conf pcs_new_after_paper2_${machine}.conf 0 ${test_launcher}"
./launcher_pcs_new_retry.sh ../machine_conf/${machine}.conf pcs_new_after_paper2_${machine}.conf 0 ${test_launcher}

cd ..

