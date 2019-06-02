HEADER:=$(shell /usr/bin/pkg-config --cflags glib-2.0)
LIBS:=$(shell /usr/bin/pkg-config --libs glib-2.0)

.PHONY : everything

everything: demo1 demo2

demo1:
	gcc -g -o demo1 demo1.c $(HEADER) $(LIBS)

demo2:
	gcc -g -o demo2 demo2.c  $(HEADER) $(LIBS)

clean:
	rm -f demo1 demo2
