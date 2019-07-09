#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <glib.h>
#include <assert.h>
#include "queue.h"

#define ROUND_UP(n, d) (((n) + (d) - 1) & -(0 ? (n) : (d)))

#define COROUTINE_STACK_SIZE (1 << 20)

#define container_of(ptr, type, member) ({                      \
        const typeof(((type *) 0)->member) *__mptr = (ptr);     \
        (type *) ((char *) __mptr - offsetof(type, member));})

#define DO_UPCAST(type, field, dev) container_of(dev, type, field)

#define coroutine_fn

typedef struct Coroutine Coroutine;

/**
 * Coroutine entry point
 *
 * When the coroutine is entered for the first time, opaque is passed in as an
 * argument.
 *
 * When this function returns, the coroutine is destroyed automatically and
 * execution continues in the caller who last entered the coroutine.
 */
typedef void coroutine_fn CoroutineEntry(void *opaque);

/**
 * Create a new coroutine
 *
 * Use qemu_coroutine_enter() to actually transfer control to the coroutine.
 * The opaque argument is passed as the argument to the entry point.
 */
Coroutine *qemu_coroutine_create(CoroutineEntry *entry, void *opaque);

/**
 * Transfer control to a coroutine
 */
void qemu_coroutine_enter(Coroutine *coroutine);

/**
 * Transfer control back to a coroutine's caller
 *
 * This function does not return until the coroutine is re-entered using
 * qemu_coroutine_enter().
 */
void coroutine_fn qemu_coroutine_yield(void);

/**
 * Return whether or not currently inside a coroutine
 *
 * This can be used to write functions that work both when in coroutine context
 * and when not in coroutine context.  Note that such functions cannot use the
 * coroutine_fn annotation since they work outside coroutine context.
 */
int qemu_in_coroutine(void);

/*
 * va_args to makecontext() must be type 'int', so passing
 * the pointer we need may require several int args. This
 * union is a quick hack to let us do that
 */
union cc_arg {
    void *p;
    int i[2];
};

typedef enum {
    COROUTINE_YIELD = 1,
    COROUTINE_TERMINATE = 2,
    COROUTINE_ENTER = 3,
} CoroutineAction;

struct Coroutine {
    CoroutineEntry *entry;
    void *entry_arg;
    Coroutine *caller;

    /* Used to catch and abort on illegal co-routine entry.
     * Will contain the name of the function that had first
     * scheduled the coroutine. */
    const char *scheduled;

    QSIMPLEQ_ENTRY(Coroutine) co_queue_next;

    /* Coroutines that should be woken up when we yield or terminate.
     * Only used when the coroutine is running.
     */
    QSIMPLEQ_HEAD(, Coroutine) co_queue_wakeup;
};

typedef struct {
    Coroutine base;
    void *stack;
    size_t stack_size;
    sigjmp_buf env;
} CoroutineUContext;
