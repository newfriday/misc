HEADER:=$(shell /usr/bin/pkg-config --cflags glib-2.0)
LIBS:=$(shell /usr/bin/pkg-config --libs glib-2.0)

.PHONY : everything

everything: qemu_main_loop qemu_main_loop_debug

qemu_main_loop: qemu_main_loop.c event_notifier.c aio.c async.c
	gcc -g -o qemu_main_loop qemu_main_loop.c event_notifier.c aio.c async.c lockcnt.c -lpthread $(HEADER) $(LIBS)

qemu_main_loop_debug: qemu_main_loop.c event_notifier.c aio.c async.c
	gcc -g -o qemu_main_loop_debug qemu_main_loop.c event_notifier.c aio.c async.c lockcnt.c -DDEBUG -lpthread $(HEADER) $(LIBS)

clean:
	rm -f qemu_main_loop qemu_main_loop_debug
