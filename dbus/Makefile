demo1:	demo1.c
	gcc -o demo1 demo1.c -l dbus-1
demo3:	demo3.c
	gcc -o demo3 -g demo3.c -l dbus-1 -I/usr/share
demo2_client:	demo2_client.c
	gcc -o demo2_client demo2_client.c -l dbus-1
demo2_server:	demo2_server.c
	gcc -o demo2_server demo2_server.c -l dbus-1

.PHONY : all

all:	demo1 demo2_client demo2_server demo3

clean:
	rm -f demo1 demo2_client demo2_server demo3
