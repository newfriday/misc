.PHONY: all

all: read_shared_memory

read_shared_memory: read_shared_memory.c
	gcc -o read_shared_memory read_shared_memory.c -lrt

clean:
	rm -f read_shared_memory
