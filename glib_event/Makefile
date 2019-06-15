HEADER:=$(shell /usr/bin/pkg-config --cflags glib-2.0)
LIBS:=$(shell /usr/bin/pkg-config --libs glib-2.0)

.PHONY : everything

everything: demo1 demo2 qemu_main_loop qemu_main_loop_debug poll_demo

demo1:
	gcc -g -o demo1 demo1.c $(HEADER) $(LIBS)

demo2:
	gcc -g -o demo2 demo2.c  $(HEADER) $(LIBS)

qemu_main_loop:
	gcc -g -o qemu_main_loop qemu_main_loop.c $(HEADER) $(LIBS)

qemu_main_loop_debug:
	gcc -g -o qemu_main_loop_debug qemu_main_loop.c -DDEBUG $(HEADER) $(LIBS)

poll_demo:
	gcc -g -o poll_demo poll_demo.c

clean:
	rm -f demo1 demo2 qemu_main_loop qemu_main_loop_debug poll_demo
