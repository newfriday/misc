/* uthread.c */
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

unsigned char *stack1, *stack2;
struct context *ctx1, *ctx2;

void schedule(struct context *prev, struct context *next)
{
    int ret;
    ret = save(prev);
    if (ret == 0) {
        restore(next);
    }
}

void func1()
{
    int i = 1;
    while (i++) {
        printf("thread 1 :%d\n", i);
        sleep(1);
        if (i%3 == 0) {
            schedule(ctx1, ctx2);
        }
    }
}

void func2()
{
    int i = 0xffff;
    while (i--) {
        printf("thread 2 :%d\n", i);
        sleep(1);
        if (i%3 == 0) {
            schedule(ctx2, ctx1);
        }
    }
}

int main(int unused1, char **unused2)
{
    int i, j, k;

    ctx1 = (struct context *)malloc(sizeof(struct context));
    ctx2 = (struct context *)malloc(sizeof(struct context));
    stack1 = (unsigned char *)malloc(4096);
    stack2 = (unsigned char *)malloc(4096);

    memset(ctx1, 0, sizeof(struct context));
    memset(ctx2, 0, sizeof(struct context));

    i = save(ctx1);
    j = save(ctx2);

    // 以下的4行是关键，用glibc的setjmp/longjmp很难做到！
    ctx1->rip = &func1;
    // 因为stack是向下生长的，所以要从高地址开始！这点很容易出错。
    ctx1->rsp = ctx1->rbp = stack1+4000;
    ctx2->rip = &func2;
    ctx2->rsp = ctx2->rbp = stack2+4000;

    // 切换到thread1的func1，内部切换了stack，并且由于修改了ctx的rip，即修改了save的返回地址，将直接进入func1的逻辑！
    k = restore(ctx1);

    return 0;
}
