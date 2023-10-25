/* cpuid.c */
/* gcc -o cpuid cpuid.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void cpuid(unsigned int index,
                         unsigned int *eax,
                         unsigned int *ebx,
                         unsigned int *ecx,
                         unsigned int *edx)
{
    asm("cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "0" (index));
}

void main (int argc, char *argv[])
{
    int eax_in;
    unsigned int eax, ebx, ecx, edx;

    eax_in = argv[1] ? atoi(argv[1]) : 0;
    cpuid(eax_in, &eax, &ebx, &ecx, &edx);

    printf("input eax = %d\n", eax_in);
    printf("eax = %x\n", eax);
    printf("ebx = %x\n", ebx);
    printf("ecx = %x\n", ecx);
    printf("edx = %x\n", edx);
}
