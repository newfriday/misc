/* test_saverestore.c */
/* gcc -c test_saverestore.c -o test_saverestore.o */
#include <stdio.h>
#include <stdlib.h>

void main(void)
{
    long ret;
    ret = cpuinfo_rbx();
    printf("rbx = %x\n", ret);

    ret = cpuinfo_rcx();
    printf("rcx = %x\n", ret);

    ret = cpuinfo_rdx();
    printf("rdx = %x\n", ret);
}
