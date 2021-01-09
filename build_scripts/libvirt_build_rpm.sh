#!/bin/bash

VERSION="6.10.0"

PWD=$(pwd)
BUILDDIR=$PWD/local-build
RPMBUILDDIR=$BUILDDIR/rpmbuild

function compress_xz {
    tar --format=posix -chf - libvirt-$VERSION | XZ_OPT="-v -T0 --e" xz -c >libvirt-$VERSION.tar.xz
    [ $? -ne 0 ] && echo "failed to compress libvirt xz package" && exit 1
}

function build_rpm {
    echo "build rpm"

    mkdir -pv rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

    mv libvirt-$VERSION.tar.xz $RPMBUILDDIR/SOURCES/

    cp $BUILDDIR/libvirt.spec $RPMBUILDDIR/SPECS/

    rpmbuild -D "_topdir $RPMBUILDDIR" \
        -D "_without_sanlock 1" \
        -D "_without_storage_gluster 1" \
        -D "_without_fuse 1" \
        -D "_without_esx 1" \
        -D "_without_lxc 1" \
        -D "_without_vbox 1" \
        -D "_without_vmware 1" \
        -D "_without_hyperv 1" \
        -D "_without_storage_zfs 1" \
        -D "_without_firewalld 1" \
        -D "_without_firewalld_zone 1" \
        -D "_without_storage_sheepdog 1" \
        -D "_with_storage_rbd 1" \
        -ba $BUILDDIR/libvirt.spec

    [ $? -eq 0 ] && exit 0
    exit 1 
}


### main ###
[ -d $BUILDDIR ] && rm -rf $BUILDDIR

mkdir $BUILDDIR

compress_xz

cp libvirt-$VERSION.tar.xz libvirt.spec $BUILDDIR 

pushd $BUILDDIR

build_rpm

popd
