# Project version: 1.0.0
# C compiler for the host machine: cc (gcc 10.3.1 "cc (GCC) 10.3.1")
# C linker for the host machine: cc ld.bfd 2.37
# Host machine cpu family: x86_64
# Host machine cpu: x86_64
# Library m found: YES
# Dependency libvirt found: YES 9.10.0 (cached)
# Dependency glib-2.0 found: YES 2.74.4 (cached)
# Dependency libvirt found: YES 9.10.0 (cached)

# project: gvm-event-monitor; c executable binary; version: output if `cat VERSION`
project('gvm-event-monitor', ['c'],
        version: files('VERSION'))
# include relative path: ./include
incdir = include_directories('include')

# get the cc compiler
cc = meson.get_compiler('c')
# find the libm library
libm = cc.find_library('m', required: true)
# set the dependencies for jsonrpc: libvirt-devel, libm
jsonrpc_deps = [dependency('libvirt'), libm]

# compile the static library: libcJSON.a
lib_jsonrpc = library('cJSON', 'util/cJSON.c', 'util/jsonrpc-s.c', 'util/jsonrpc-c.c',
                      include_directories: incdir,
                      dependencies: jsonrpc_deps,
                      version: '1.0.0')

# set the dependencies for binary
gvm_deps = [dependency('glib-2.0'), dependency('libvirt')]

# compile the executable binary: gvm-event-monitor
executable('gvm-event-monitor', 'gvm-event-monitor.c', 'util/misc.c',
           link_with: lib_jsonrpc,
           dependencies: gvm_deps,
           include_directories: incdir)
