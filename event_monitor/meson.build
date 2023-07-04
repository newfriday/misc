project('gvm-event-monitor', ['c'],
        version: files('VERSION'))
incdir = include_directories('include')

lib_cJSON = library('cJSON', 'util/cJSON.c',
                    include_directories: incdir,
                    version: '1.0.0')

deps = [dependency('glib-2.0'), dependency('libvirt')]
executable('gvm-event-monitor', 'gvm-event-monitor.c', 'util/misc.c',
           link_with: lib_cJSON,
           dependencies: deps,
           include_directories: incdir)
