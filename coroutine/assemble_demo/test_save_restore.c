/* test_saverestore.c */
/* gcc -c test_saverestore.c -o test_saverestore.o */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct context {
    unsigned long rsp;
    unsigned long rbp;
    unsigned long rip;
    unsigned long r12;
    unsigned long r13;
    unsigned long r14;
    unsigned long r15;
};

struct context ctx;

int main()
{
    int ret;
    ret = save(&ctx);
    if (ret == 0) {
        printf("from setjmp\n");
        restore(&ctx);
    } else if (ret == 1) {
        printf("from longjmp\n");
    }
}
