#include "coroutine.h"

/**
 * Per-thread coroutine bookkeeping
 */
static __thread CoroutineUContext leader;
static __thread Coroutine *current;

/* This function is marked noinline to prevent GCC from inlining it
 * into coroutine_trampoline(). If we allow it to do that then it
 * hoists the code to get the address of the TLS variable "current"
 * out of the while() loop. This is an invalid transformation because
 * the sigsetjmp() call may be called when running thread A but
 * return in thread B, and so we might be in a different thread
 * context each time round the loop.
 */
CoroutineAction __attribute__((noinline))
qemu_coroutine_switch(Coroutine *from_, Coroutine *to_,
                      CoroutineAction action)
{
    CoroutineUContext *from = DO_UPCAST(CoroutineUContext, base, from_);
    CoroutineUContext *to = DO_UPCAST(CoroutineUContext, base, to_);
    int ret;

    current = to_;

    ret = sigsetjmp(from->env, 0);
    if (ret == 0) {
        siglongjmp(to->env, action);
    }

    return ret;
}

static void coroutine_trampoline(int i0, int i1)
{
    union cc_arg arg;
    CoroutineUContext *self;
    Coroutine *co;

    arg.i[0] = i0;
    arg.i[1] = i1;
    self = arg.p;
    co = &self->base;

    /* Initialize longjmp environment and switch back the caller */
    if (!sigsetjmp(self->env, 0)) {
        siglongjmp(*(sigjmp_buf *)co->entry_arg, 1);
    }

    while (1) {
        co->entry(co->entry_arg);
        qemu_coroutine_switch(co, co->caller, COROUTINE_TERMINATE);
    }
}

void qemu_free_stack(void *stack, size_t sz)
{
    munmap(stack, sz);
}


void *qemu_alloc_stack(size_t *sz)
{
    void *ptr, *guardpage;
    int flags;
    size_t pagesz = getpagesize();

    /* adjust stack size to a multiple of the page size */
    *sz = ROUND_UP(*sz, pagesz);
    /* allocate one extra page for the guard page */
    *sz += pagesz;

    flags = MAP_PRIVATE | MAP_ANONYMOUS;

    ptr = mmap(NULL, *sz, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("failed to allocate memory for stack");
        abort();
    }

    /* stack grows down */
    guardpage = ptr;

    if (mprotect(guardpage, pagesz, PROT_NONE) != 0) {
        perror("failed to set up stack guard page");
        abort();
    }

    return ptr;
}

Coroutine *qemu_coroutine_new(void)
{
    CoroutineUContext *co;
    ucontext_t old_uc, uc;
    sigjmp_buf old_env;
    union cc_arg arg = {0};

    /* The ucontext functions preserve signal masks which incurs a
     * system call overhead.  sigsetjmp(buf, 0)/siglongjmp() does not
     * preserve signal masks but only works on the current stack.
     * Since we need a way to create and switch to a new stack, use
     * the ucontext functions for that but sigsetjmp()/siglongjmp() for
     * everything else.
     */

    if (getcontext(&uc) == -1) {
        abort();
    }

    co = g_malloc0(sizeof(*co));
    co->stack_size = COROUTINE_STACK_SIZE;
    co->stack = qemu_alloc_stack(&co->stack_size);
    co->base.entry_arg = &old_env; /* stash away our jmp_buf */

    uc.uc_link = &old_uc;
    uc.uc_stack.ss_sp = co->stack;
    uc.uc_stack.ss_size = co->stack_size;
    uc.uc_stack.ss_flags = 0;

    arg.p = co;

    makecontext(&uc, (void (*)(void))coroutine_trampoline,
                2, arg.i[0], arg.i[1]);

    /* swapcontext() in, siglongjmp() back out */
    if (!sigsetjmp(old_env, 0)) {
        swapcontext(&old_uc, &uc);
    }

    return &co->base;
}

Coroutine *qemu_coroutine_create(CoroutineEntry *entry, void *opaque)
{
    Coroutine *co = NULL;

    co = qemu_coroutine_new();

    co->entry = entry;
    co->entry_arg = opaque;
    QSIMPLEQ_INIT(&co->co_queue_wakeup);
    return co;
}

int qemu_in_coroutine(void)
{
    return current && current->caller;
}

Coroutine *qemu_coroutine_self(void)
{
    if (!current) {
        current = &leader.base;
    }
    return current;
}

static void qemu_coroutine_delete(Coroutine *co_)
{
    CoroutineUContext *co = DO_UPCAST(CoroutineUContext, base, co_);

    qemu_free_stack(co->stack, co->stack_size);
    g_free(co);
}

static void coroutine_delete(Coroutine *co)
{
    co->caller = NULL;
    qemu_coroutine_delete(co);
}

void qemu_aio_coroutine_enter(Coroutine *co)
{
    QSIMPLEQ_HEAD(, Coroutine) pending = QSIMPLEQ_HEAD_INITIALIZER(pending);
    Coroutine *from = qemu_coroutine_self();

    QSIMPLEQ_INSERT_TAIL(&pending, co, co_queue_next);

    /* Run co and any queued coroutines */
    while (!QSIMPLEQ_EMPTY(&pending)) {
        Coroutine *to = QSIMPLEQ_FIRST(&pending);
        CoroutineAction ret;

        /* Cannot rely on the read barrier for to in aio_co_wake(), as there are
         * callers outside of aio_co_wake() */
        const char *scheduled = to->scheduled;

        QSIMPLEQ_REMOVE_HEAD(&pending, co_queue_next);

        /* if the Coroutine has already been scheduled, entering it again will
         * cause us to enter it twice, potentially even after the coroutine has
         * been deleted */
        if (scheduled) {
            fprintf(stderr,
                    "%s: Co-routine was already scheduled in '%s'\n",
                    __func__, scheduled);
            abort();
        }

        if (to->caller) {
            fprintf(stderr, "Co-routine re-entered recursively\n");
            abort();
        }

        to->caller = from;

        ret = qemu_coroutine_switch(from, to, COROUTINE_ENTER);

        /* Queued coroutines are run depth-first; previously pending coroutines
         * run after those queued more recently.
         */
        QSIMPLEQ_PREPEND(&pending, &to->co_queue_wakeup);

        switch (ret) {
        case COROUTINE_YIELD:
            break;
        case COROUTINE_TERMINATE:
            coroutine_delete(to);
            break;
        default:
            abort();
        }
    }
}

void qemu_coroutine_enter(Coroutine *co)
{
    qemu_aio_coroutine_enter(co);
}

int qemu_coroutine_entered(Coroutine *co)
{
    return co->caller ? 1 : 0;
}

void aio_co_enter(struct Coroutine *co)
{
    if (qemu_in_coroutine()) {
        Coroutine *self = qemu_coroutine_self();
        assert(self != co);
        QSIMPLEQ_INSERT_TAIL(&self->co_queue_wakeup, co, co_queue_next);
    } else {
        qemu_aio_coroutine_enter(co);
    }
}

void coroutine_fn qemu_coroutine_yield(void)
{
    Coroutine *self = qemu_coroutine_self();
    Coroutine *to = self->caller;

    if (!to) {
        fprintf(stderr, "Co-routine is yielding to no one\n");
        abort();
    }

    self->caller = NULL;
    qemu_coroutine_switch(self, to, COROUTINE_YIELD);
}

