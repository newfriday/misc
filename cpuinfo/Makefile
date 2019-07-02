.PHONY : everything

everything : test_cpuinfo cpuid

test_cpuinfo: test_cpuinfo.o cpuinfo.o
	gcc -g -o test_cpuinfo test_cpuinfo.o cpuinfo.o
test_cpuinfo.o: test_cpuinfo.c
	gcc -g -c -o test_cpuinfo.o test_cpuinfo.c
cpuinfo.o: cpuinfo.S
	as --64 -o cpuinfo.o cpuinfo.S

cpuid: cpuid.c
	gcc -g -o cpuid cpuid.c

clean:
	rm -f *.o test_cpuinfo cpuid
