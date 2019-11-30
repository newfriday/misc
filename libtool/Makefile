.PHONY: everything

everything: libzlib.la

zlib.lo: zlib.c
	libtool --mode=compile gcc -c $<

libzlib.la: zlib.lo
	libtool --mode=link gcc -o $@ $< -rpath /tmp -lz

install: libzlib.la
	libtool --mode=install install -c libzlib.la /tmp
	libtool -n --mode=finish /tmp

main.lo: main.c
	libtool --mode=compile gcc -c main.c

main: main.lo
	libtool --mode=link gcc -o main main.lo /tmp/libzlib.la

main_plugin: main_plugin.c
	gcc -o main_plugin main_plugin.c -ldl

uninstall:
	libtool --mode=uninstall rm /tmp/libzlib.la

clean:
	libtool --mode=clean rm zlib.lo
	libtool --mode=clean rm main.lo main
