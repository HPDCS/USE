#!/bin/bash

machine="capitano"
machine_thread_list=${machine}_thread_list
test_launcher=0

cd pcs_new
echo "./launcher_pcs_new_retry.sh ../machine_conf/${machine}.conf pcs_new_after_paper1_${machine}.conf 0 ${test_launcher}"
./launcher_pcs_new_retry.sh ../machine_conf/${machine}.conf pcs_new_after_paper1_${machine}.conf 0 ${test_launcher}

cd ..

