#!/bin/bash

machine="totti"
machine_thread_list=${machine}_thread_list
test_launcher=0

cd pcs_new
./launcher_pcs_new_retry.sh ../machine_conf/${machine}.conf pcs_new_after_paper1.conf 0 ${test_launcher}
./launcher_pcs_new_retry.sh ../machine_conf/${machine}.conf pcs_new_after_paper2.conf 0 ${test_launcher}
./launcher_pcs_new_retry.sh ../machine_conf/${machine}.conf pcs_new_after_paper3.conf 0 ${test_launcher}

cd ..

cd phold
./launcher_phold_retry.sh ../machine_conf/${machine_thread_list}.conf phold_after_paper1.conf 0 ${test_launcher}

cd ..

cd phold_multicast
./launcher_phold_multicast_retry.sh ../machine_conf/${machine}.conf phold_multicast_after_paper1.conf 0 ${test_launcher}

cd ..


cd phold_nears
./launcher_phold_nears_retry.sh ../machine_conf/${machine}.conf phold_nears_after_paper1.conf 0 ${test_launcher}

cd ..

