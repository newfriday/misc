/* cpuid.c */
/* gcc -o cpuid cpuid.c */
#include <stdio.h>
#include <stdlib.h>

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

void main(void)
{
    unsigned int eax, ebx, ecx, edx;

    cpuid(0, &eax, &ebx, &ecx, &edx);

    printf("eax = %x\n", eax);
    printf("ebx = %x\n", ebx);
    printf("ecx = %x\n", ecx);
    printf("edx = %x\n", edx);
}
