project('gvm-event-monitor', ['c'],
        version: files('VERSION'))
incdir = include_directories('include')
deps = [dependency('glib-2.0'), dependency('libvirt')]
executable('gvm-event-monitor', 'gvm-event-monitor.c', 'util/misc.c',
           dependencies: deps,
           include_directories: incdir)
