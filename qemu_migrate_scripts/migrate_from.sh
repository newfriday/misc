#!/bin/bash

/usr/bin/qemu-system-x86_64 \
    -display none -vga none \
    -name guest=migrate_src,debug-threads=on \
    -monitor stdio \
    -accel kvm,dirty-ring-size=65536 -cpu host \
    -kernel /home/work/fast_qemu/vmlinuz-5.13.0-rc4+ \
    -initrd /home/work/fast_qemu/initrd-stress.img \
    -append "noapic edd=off printk.time=1 noreplace-smp cgroup_disable=memory pci=noearly console=ttyS0 debug ramsize=4" \
    -chardev file,id=charserial0,path=/var/log/mig_dst_console.log\
    -serial chardev:charserial0 \
    -D /var/log/mig_dst.log \
    -m 4096 -smp 1 -device sga \
    -incoming tcp:192.168.31.195:9000
