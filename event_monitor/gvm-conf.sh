#!/bin/bash
SRCDIR=$(pwd)
BUILDDIR=$SRCDIR/build

# compile only
MODE=2

function make_autoreconf {
    pushd $BUILDDIR
    meson setup --wipe
    meson setup
    meson configure --debug -Ddefault_library=static
    meson compile
    popd
    exit 0
}

function compile_only {
    pushd $BUILDDIR
    meson setup --wipe
    meson configure --debug -Ddefault_library=static
    meson compile
    popd
    exit 0
}

function main {
    while getopts "ac" arg; do
        case $arg in
            a)
        shift
                echo "autoreconf before make"
                MODE=1
                ;;
            c)
        shift
                echo "compile only"
                MODE=2
                ;;
           ?)
                echo "invalid argument: $arg"
                exit 2
                ;;
        esac
    done

    [ $MODE -eq 1 ] && make_autoreconf
    [ $MODE -eq 2 ] && compile_only

    echo "invalid MODE: $MODE"
    exit 1
}

mkdir -p $BUILDDIR
main $@
