HEADER:=$(shell /usr/bin/pkg-config --cflags glib-2.0)
LIBS:=$(shell /usr/bin/pkg-config --libs glib-2.0)

main: main.c coroutine.c
	gcc -g -o main main.c coroutine.c $(HEADER) $(LIBS) -lpthread

clean:
	rm -f main
