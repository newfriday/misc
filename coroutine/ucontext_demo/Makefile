all : main simple

main : main.c coroutine.c
	gcc -g -Wall -o $@ $^

simple: simple.c
	gcc -g -Wall -o simple simple.c

clean :
	rm main simple
