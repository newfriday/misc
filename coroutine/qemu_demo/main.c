#include "coroutine.h"

static void coroutine_fn verify_entered_step_2(void *opaque)
{
    Coroutine *caller = (Coroutine *)opaque;

    assert(qemu_coroutine_entered(caller));
    fprintf(stdout, "coroutine: verify_entered_step_1, entered\n");

    assert(qemu_coroutine_entered(qemu_coroutine_self()));
    fprintf(stdout, "coroutine: verify_entered_step_2, entered\n");

    fprintf(stdout, "coroutine: verify_entered_step_2, ready to yield to verify_entered_step_1\n");
    qemu_coroutine_yield();

    fprintf(stdout, "coroutine: verify_entered_step_2, return frome yield\n");
    /* Once more to check it still works after yielding */
    assert(qemu_coroutine_entered(caller));
    fprintf(stdout, "coroutine: verify_entered_step_1, entered\n");
    assert(qemu_coroutine_entered(qemu_coroutine_self()));
    fprintf(stdout, "coroutine: verify_entered_step_2, entered\n");
}

static void coroutine_fn verify_entered_step_1(void *opaque)
{
    Coroutine *self = qemu_coroutine_self();
    Coroutine *coroutine;

    assert(qemu_coroutine_entered(self));
    fprintf(stdout, "coroutine: verify_entered_step_1, entered\n");

    coroutine = qemu_coroutine_create(verify_entered_step_2, self);

    fprintf(stdout, "coroutine: verify_entered_step_1, ready to enter verify_entered_step_2\n");
    assert(!qemu_coroutine_entered(coroutine));

    qemu_coroutine_enter(coroutine);
    fprintf(stdout, "coroutine: verify_entered_step_1, verify_entered_step_2 has yield\n");

    assert(!qemu_coroutine_entered(coroutine));

    fprintf(stdout, "coroutine: verify_entered_step_1, enter verify_entered_step_2 again\n");
    qemu_coroutine_enter(coroutine);
    fprintf(stdout, "coroutine: verify_entered_step_1, verify_entered_step_2 done\n");
}

static void test_entered(void)
{
    Coroutine *coroutine;

    coroutine = qemu_coroutine_create(verify_entered_step_1, NULL);
    assert(!qemu_coroutine_entered(coroutine));
    fprintf(stdout, "test_entered: ready to enter coroutine: verify_entered_step_1\n");
    qemu_coroutine_enter(coroutine);
    fprintf(stdout, "test_entered: verify_entered_step_1 done\n");
}

static void coroutine_fn verify_in_coroutine(void *opaque)
{
    assert(qemu_in_coroutine());
    fprintf(stdout, "verify_in_coroutine: entered\n");
}

static void test_in_coroutine(void)
{
    Coroutine *coroutine;

    assert(!qemu_in_coroutine());
    fprintf(stdout, "test_in_coroutine: not in coroutine\n");

    coroutine = qemu_coroutine_create(verify_in_coroutine, NULL);
    fprintf(stdout, "test_in_coroutine: ready to enter verify_in_coroutine\n");
    qemu_coroutine_enter(coroutine);
    fprintf(stdout, "test_in_coroutine: verify_in_coroutine done\n");
}

static void
coroutine_fn yield_5_times(void *opaque)
{
    int *done = opaque;
    int i;

    for (i = 0; i < 5; i++) {
        fprintf(stdout, "yield_5_times: ready to yield to test_yield, count %d\n", i);
        qemu_coroutine_yield();
        fprintf(stdout, "yield_5_times: return frome yield, count %d\n", i);
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
        fprintf(stdout, "test_yield: ready to enter yield_5_times, count %d\n", i);
        qemu_coroutine_enter(coroutine);
        fprintf(stdout, "test_yield: yield_5_times has yield, count %d\n", i);
        i++;
    }
}

static void coroutine_fn verify_self(void *opaque)
{
    Coroutine **p_co = opaque;
    assert(qemu_coroutine_self() == *p_co);
    fprintf(stdout, "verify_self: current is myself\n");
}

static void test_self(void)
{
    Coroutine *coroutine;

    coroutine = qemu_coroutine_create(verify_self, &coroutine);
    fprintf(stdout, "test_self: ready to enter verify_self\n");
    qemu_coroutine_enter(coroutine);
    fprintf(stdout, "test_self: verify_self done\n");
}

void main(void)
{
    fprintf(stdout, "test self ......\n");
    test_self();
    fprintf(stdout, "\n");
    fprintf(stdout, "\n");

    fprintf(stdout, "test yield ......\n");
    test_yield();
    fprintf(stdout, "\n");
    fprintf(stdout, "\n");

    fprintf(stdout, "test in coroutine ......\n");
    test_in_coroutine();
    fprintf(stdout, "\n");
    fprintf(stdout, "\n");

    fprintf(stdout, "test entered......\n");
    test_entered();
}
