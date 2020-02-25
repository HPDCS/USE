#!/bin/bash 
export DEBIAN_FRONTEND=noninteractive 
apt update; apt upgrade -y; apt install libcap2 libcap-dev g++ gcc gdb make libnuma-dev git time linux-tools-`uname -r` linux-cloud-tools-`uname -r` -y;
echo -1 > /proc/sys/kernel/perf_event_paranoid
useradd romolo -m -s /bin/bash
mkdir /mnt/myresults
git clone https://github.com/packethost/packet-block-storage.git
su - use

#git config --global user.email "romolo.marotta@gmail.com"
#git config --global user.name "Romolo"
#it clone --branch dev https://romolo89@bitbucket.org/romolo89/nbcqueue.git

