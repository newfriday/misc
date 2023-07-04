project('gvm-event-monitor', ['c'],
        version: files('VERSION'))
incdir = include_directories('include')

cc = meson.get_compiler('c')
libm = cc.find_library('m', required: true)
jsonrpc_deps = [dependency('libvirt'), libm]

lib_jsonrpc = library('cJSON', 'util/cJSON.c', 'util/jsonrpc-s.c', 'util/jsonrpc-c.c',
                      include_directories: incdir,
                      dependencies: jsonrpc_deps,
                      version: '1.0.0')

gvm_deps = [dependency('glib-2.0'), dependency('libvirt')]

executable('gvm-event-monitor', 'gvm-event-monitor.c', 'util/misc.c',
           link_with: lib_jsonrpc,
           dependencies: gvm_deps,
           include_directories: incdir)
