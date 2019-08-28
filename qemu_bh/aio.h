#include <glib.h>
#include "event_notifier.h"
#include "util.h"

/*
 * List definitions.
 */
#define QLIST_HEAD(name, type)                                          \
struct name {                                                           \
        struct type *lh_first;  /* first element */                     \
}

#define QLIST_ENTRY(type)                                               \
struct {                                                                \
        struct type *le_next;   /* next element */                      \
        struct type **le_prev;  /* address of previous next element */  \
}

#define QLIST_FOREACH(var, head, field)                                 \
        for ((var) = ((head)->lh_first);                                \
                (var);                                                  \
                (var) = ((var)->field.le_next))

#define QLIST_INSERT_HEAD(head, elm, field) do {                        \
            if (((elm)->field.le_next = (head)->lh_first) != NULL)          \
                    (head)->lh_first->field.le_prev = &(elm)->field.le_next;\
            (head)->lh_first = (elm);                                       \
            (elm)->field.le_prev = &(head)->lh_first;                       \
} while (/*CONSTCOND*/0)

#define QLIST_REMOVE(elm, field) do {                                   \
        if ((elm)->field.le_next != NULL)                               \
                (elm)->field.le_next->field.le_prev =                   \
                    (elm)->field.le_prev;                               \
        *(elm)->field.le_prev = (elm)->field.le_next;                   \
} while (/*CONSTCOND*/0)

struct QemuLockCnt {
    unsigned count;
};
typedef struct QemuLockCnt QemuLockCnt;
typedef struct QEMUBH QEMUBH;

typedef void QEMUBHFunc(void *opaque);
typedef bool AioPollFn(void *opaque);
typedef void IOHandler(void *opaque);

struct AioContext {
    GSource source;

    /* Used by AioContext users to protect from multi-threaded access.  */
    QLIST_HEAD(, AioHandler) aio_handlers;

    /* Used to avoid unnecessary event_notifier_set calls in aio_notify;
     * accessed with atomic primitives.  If this field is 0, everything
     * (file descriptors, bottom halves, timers) will be re-evaluated
     * before the next blocking poll(), thus the event_notifier_set call
     * can be skipped.  If it is non-zero, you may need to wake up a
     * concurrent aio_poll or the glib main event loop, making
     * event_notifier_set necessary.
     *
     * Bit 0 is reserved for GSource usage of the AioContext, and is 1
     * between a call to aio_ctx_prepare and the next call to aio_ctx_check.
     * Bits 1-31 simply count the number of active calls to aio_poll
     * that are in the prepare or poll phase.
     *
     * that are in the prepare or poll phase.
     *
     * The GSource and aio_poll must use a different mechanism because
     * there is no certainty that a call to GSource's prepare callback
     * (via g_main_context_prepare) is indeed followed by check and
     * dispatch.  It's not clear whether this would be a bug, but let's
     * play safe and allow it---it will just cause extra calls to
     * event_notifier_set until the next call to dispatch.
     *
     * Instead, the aio_poll calls include both the prepare and the
     * dispatch phase, hence a simple counter is enough for them.
     */
    uint32_t notify_me;

    /* A lock to protect between QEMUBH and AioHandler adders and deleter,
     * and to ensure that no callbacks are removed while we're walking and
     * dispatching them.
     */
    QemuLockCnt list_lock;

    /* Anchor of the list of Bottom Halves belonging to the context */
    struct QEMUBH *first_bh;

    /* Used by aio_notify.
     *
     * "notified" is used to avoid expensive event_notifier_test_and_clear
     * calls.  When it is clear, the EventNotifier is clear, or one thread
     * is going to clear "notified" before processing more events.  False
     * positives are possible, i.e. "notified" could be set even though the
     * EventNotifier is clear.
     *
     * Note that event_notifier_set *cannot* be optimized the same way.  For
     * more information on the problem that would result, see "#ifdef BUG2"
     * in the docs/aio_notify_accept.promela formal model.
     */
    bool notified;
    EventNotifier notifier;

    QEMUBH *co_schedule_bh;
};

struct AioHandler {
    GPollFD pfd;
    IOHandler *io_read;
    IOHandler *io_write;
    AioPollFn *io_poll;
    IOHandler *io_poll_begin;
    IOHandler *io_poll_end;
    int deleted;
    void *opaque;
    QLIST_ENTRY(AioHandler) node;
};

typedef struct AioContext AioContext;
typedef struct AioHandler AioHandler;

static AioContext *qemu_aio_context;
static AioContext *iohandler_ctx;
static GArray *gpollfds;
static int max_priority;

gboolean
aio_prepare(AioContext *ctx);

gboolean
aio_pending(AioContext *ctx);

gboolean
aio_dispatch_handlers(AioContext *ctx);

void aio_set_fd_handler(AioContext *ctx,
                        int fd,
                        IOHandler *io_read,
                        IOHandler *io_write,
                        AioPollFn *io_poll,
                        void *opaque);

