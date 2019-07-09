#include "coroutine.h"

static void
coroutine_fn yield_5_times(void *opaque)
{
    int *done = opaque;
    int i;

    for (i = 0; i < 5; i++) {
        qemu_coroutine_yield();
    }
    *done = 1;
}

static void
test_yield(void)
{
    Coroutine *coroutine;
    int done = 0;
    int i = -1; /* one extra time to return from coroutine */

    coroutine = qemu_coroutine_create(yield_5_times, &done);
    while (!done) {
        qemu_coroutine_enter(coroutine);
        i++;
    }
    fprintf(stdout, "i: %d\n", i);
}

static void coroutine_fn verify_self(void *opaque)
{
    Coroutine **p_co = opaque;
    assert(qemu_coroutine_self() == *p_co);
}

static void test_self(void)
{
    Coroutine *coroutine;

    coroutine = qemu_coroutine_create(verify_self, &coroutine);
    qemu_coroutine_enter(coroutine);
}

void main(void)
{
    test_self();
    test_yield();
}
