#!/bin/bash

/usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 /home/romolo/certs/my_signing_key.priv /home/romolo/certs/my_signing_key_pub.der /home/romolo/git/USE/Debug/src/idt_patcher/idt_patcher.ko
/usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 /home/romolo/certs/my_signing_key.priv /home/romolo/certs/my_signing_key_pub.der /home/romolo/git/USE/Debug/src/syscall_table_patcher/syscall_table_patcher.ko
/usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 /home/romolo/certs/my_signing_key.priv /home/romolo/certs/my_signing_key_pub.der /home/romolo/git/USE/Debug/src/kallsyms_lookup_name_exporter/kln_exporter.ko
/usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 /home/romolo/certs/my_signing_key.priv /home/romolo/certs/my_signing_key_pub.der /home/romolo/git/USE/Debug/src/trap-based-user-context/trap-based-usercontext.ko
