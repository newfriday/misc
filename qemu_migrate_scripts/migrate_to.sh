#!/bin/bash
/usr/bin/qemu-system-x86_64 \
    -display none -vga none \
    -name guest=mig_src,debug-threads=on \
    -monitor stdio \
    -accel kvm,dirty-ring-size=4096 -cpu host \
    -kernel /home/work/fast_qemu/vmlinuz-5.13.0-rc4+ \
    -initrd /home/work/fast_qemu/initrd-stress.img \
    -append "noapic edd=off printk.time=1 noreplace-smp cgroup_disable=memory pci=noearly console=ttyS0 debug ramsize=500 ratio=1 sleep=1" \
    -chardev file,id=charserial0,path=/var/log/mig_src_console.log \
    -serial chardev:charserial0 \
    -qmp unix:/tmp/qmp-sock,server,nowait \
    -D /var/log/mig_src.log \
    --trace events=/home/work/fast_qemu/events \
    -m 4096 -smp 2 -device sga
