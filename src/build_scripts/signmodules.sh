#!/bin/bash

ver=Release

/usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 /home/romolo/certs/my_signing_key.priv /home/romolo/certs/my_signing_key_pub.der /home/romolo/git/USE/$ver/src/idt_patcher/idt_patcher.ko
/usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 /home/romolo/certs/my_signing_key.priv /home/romolo/certs/my_signing_key_pub.der /home/romolo/git/USE/$ver/src/syscall_table_patcher/syscall_table_patcher.ko
/usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 /home/romolo/certs/my_signing_key.priv /home/romolo/certs/my_signing_key_pub.der /home/romolo/git/USE/$ver/src/kallsyms_lookup_name_exporter/kln_exporter.ko
/usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 /home/romolo/certs/my_signing_key.priv /home/romolo/certs/my_signing_key_pub.der /home/romolo/git/USE/$ver/src/trap-based-user-context/trap-based-usercontext.ko
