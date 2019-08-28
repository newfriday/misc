#include <glib.h>
#include <assert.h>
#include "atomic.h"
#include "aio.h"

#define container_of(ptr, type, member) ({                      \
        const typeof(((type *) 0)->member) *__mptr = (ptr);     \
        (type *) ((char *) __mptr - offsetof(type, member));})

/***********************************************************/
/* bottom halves (can be seen as timers which expire ASAP) */

struct QEMUBH {
    AioContext *ctx;
    QEMUBHFunc *cb;
    void *opaque;
    QEMUBH *next;
    bool scheduled;
    bool idle;
    bool deleted;
};

void aio_notify(AioContext *ctx)
{
    /* Write e.g. bh->scheduled before reading ctx->notify_me.  Pairs
     * with atomic_or in aio_ctx_prepare or atomic_add in aio_poll.
     */
    smp_mb();
    if (ctx->notify_me) {
        event_notifier_set(&ctx->notifier);
        atomic_mb_set(&ctx->notified, true);
    }
}

void aio_notify_accept(AioContext *ctx)
{
    if (atomic_xchg(&ctx->notified, false)) {
        event_notifier_test_and_clear(&ctx->notifier);
    }
}

void aio_bh_schedule_oneshot(AioContext *ctx, QEMUBHFunc *cb, void *opaque)
{
    QEMUBH *bh;
    bh = g_new(QEMUBH, 1);
    *bh = (QEMUBH){
        .ctx = ctx,
        .cb = cb,
        .opaque = opaque,
    };
    qemu_lockcnt_lock(&ctx->list_lock);
    bh->next = ctx->first_bh;
    bh->scheduled = 1;
    bh->deleted = 1;
    /* Make sure that the members are ready before putting bh into list */
    smp_wmb();
    ctx->first_bh = bh;
    qemu_lockcnt_unlock(&ctx->list_lock);
    aio_notify(ctx);
}

QEMUBH *aio_bh_new(AioContext *ctx, QEMUBHFunc *cb, void *opaque)
{
    QEMUBH *bh;
    bh = g_new(QEMUBH, 1);
    *bh = (QEMUBH){
        .ctx = ctx,
        .cb = cb,
        .opaque = opaque,
    };
    qemu_lockcnt_lock(&ctx->list_lock);
    bh->next = ctx->first_bh;
    /* Make sure that the members are ready before putting bh into list */
    smp_wmb();
    ctx->first_bh = bh;
    qemu_lockcnt_unlock(&ctx->list_lock);
    return bh;
}

void aio_bh_call(QEMUBH *bh)
{
    bh->cb(bh->opaque);
}

/* Multiple occurrences of aio_bh_poll cannot be called concurrently.
 * The count in ctx->list_lock is incremented before the call, and is
 * not affected by the call.
 */
int aio_bh_poll(AioContext *ctx)
{
    QEMUBH *bh, **bhp, *next;
    int ret;
    bool deleted = false;

    ret = 0;
    for (bh = atomic_rcu_read(&ctx->first_bh); bh; bh = next) {
        next = atomic_rcu_read(&bh->next);
        /* The atomic_xchg is paired with the one in qemu_bh_schedule.  The
         * implicit memory barrier ensures that the callback sees all writes
         * done by the scheduling thread.  It also ensures that the scheduling
         * thread sees the zero before bh->cb has run, and thus will call
         * aio_notify again if necessary.
         */
        if (atomic_xchg(&bh->scheduled, 0)) {
            /* Idle BHs don't count as progress */
            if (!bh->idle) {
                ret = 1;
            }
            bh->idle = 0;
            aio_bh_call(bh);
        }
        if (bh->deleted) {
            deleted = true;
        }
    }

    /* remove deleted bhs */
    if (!deleted) {
        return ret;
    }

    if (qemu_lockcnt_dec_if_lock(&ctx->list_lock)) {
        bhp = &ctx->first_bh;
        while (*bhp) {
            bh = *bhp;
            if (bh->deleted && !bh->scheduled) {
                *bhp = bh->next;
                g_free(bh);
            } else {
                bhp = &bh->next;
            }
        }
        qemu_lockcnt_inc_and_unlock(&ctx->list_lock);
    }
    return ret;
}

void qemu_bh_schedule_idle(QEMUBH *bh)
{
    bh->idle = 1;
    /* Make sure that idle & any writes needed by the callback are done
     * before the locations are read in the aio_bh_poll.
     */
    atomic_mb_set(&bh->scheduled, 1);
}

void qemu_bh_schedule(QEMUBH *bh)
{
    AioContext *ctx;

    ctx = bh->ctx;
    bh->idle = 0;
    /* The memory barrier implicit in atomic_xchg makes sure that:
     * 1. idle & any writes needed by the callback are done before the
     *    locations are read in the aio_bh_poll.
     * 2. ctx is loaded before scheduled is set and the callback has a chance
     *    to execute.
     */
    if (atomic_xchg(&bh->scheduled, 1) == 0) {
        aio_notify(ctx);
    }
}

/* This func is async.
 */
void qemu_bh_cancel(QEMUBH *bh)
{
    atomic_mb_set(&bh->scheduled, 0);
}

/* This func is async.The bottom half will do the delete action at the finial
 * end.
 */
void qemu_bh_delete(QEMUBH *bh)
{
    bh->scheduled = 0;
    bh->deleted = 1;
}

static gboolean
aio_ctx_prepare(GSource *source, gint    *timeout)
{
#ifdef DEBUG
    g_print("%s\n", __FUNCTION__);
#endif
    AioContext *ctx = (AioContext *) source;

    atomic_or(&ctx->notify_me, 1);

    /* default timeout 10ms */
    *timeout = 10;

    if (aio_prepare(ctx)) {
        *timeout = 0;
    }

    return *timeout == 0;
}

static gboolean
aio_ctx_check(GSource *source)
{
#ifdef DEBUG
    g_print("%s\n", __FUNCTION__);
#endif
    AioContext *ctx = (AioContext *) source;
    QEMUBH *bh;

    atomic_and(&ctx->notify_me, ~1);
    aio_notify_accept(ctx);

    for (bh = ctx->first_bh; bh; bh = bh->next) {
        if (bh->scheduled) {
            return true;
        }
    }

    return aio_pending(ctx);
}


static void
aio_dispatch(AioContext *ctx)
{
    qemu_lockcnt_inc(&ctx->list_lock);
    aio_bh_poll(ctx);
    aio_dispatch_handlers(ctx);
    qemu_lockcnt_dec(&ctx->list_lock);
}

static gboolean
aio_ctx_dispatch(GSource     *source,
                 GSourceFunc  callback,
                 gpointer     user_data)
{
#ifdef DEBUG
    g_print("%s\n", __FUNCTION__);
#endif
    AioContext *ctx = (AioContext *) source;

    aio_dispatch(ctx);
    return TRUE;
}

static void
aio_ctx_finalize(GSource     *source)
{
#ifdef DEBUG
    g_print("%s\n", __FUNCTION__);
#endif
    AioContext *ctx = (AioContext *) source;

    qemu_bh_delete(ctx->co_schedule_bh);

    qemu_lockcnt_lock(&ctx->list_lock);
    while (ctx->first_bh) {
        QEMUBH *next = ctx->first_bh->next;

        /* qemu_bh_delete() must have been called on BHs in this AioContext */
        assert(ctx->first_bh->deleted);

        g_free(ctx->first_bh);
        ctx->first_bh = next;
    }
    qemu_lockcnt_unlock(&ctx->list_lock);

    aio_set_event_notifier(ctx, &ctx->notifier, NULL);
    event_notifier_cleanup(&ctx->notifier);
}

static GSourceFuncs
aio_source_funcs = {
    aio_ctx_prepare,
    aio_ctx_check,
    aio_ctx_dispatch,
    aio_ctx_finalize
};

static void event_notifier_dummy_cb(EventNotifier *e)
{

}

/* Returns true if aio_notify() was called (e.g. a BH was scheduled) */
static bool event_notifier_poll(void *opaque)
{
    EventNotifier *e = opaque;
    AioContext *ctx = container_of(e, AioContext, notifier);

    return atomic_read(&ctx->notified);
}

AioContext *
aio_context_new(void)
{
    int ret;
    AioContext *ctx;

    ctx = (AioContext *) g_source_new(&aio_source_funcs, sizeof(AioContext));

    ret = event_notifier_init(&ctx->notifier, false);
    if (ret < 0) {
        g_print("%s Failed to initialize event notifier\n", __FUNCTION__);
        goto fail;
    }
    qemu_lockcnt_init(&ctx->list_lock);

    aio_set_event_notifier(ctx, &ctx->notifier,
                           (EventNotifierHandler *)
                           event_notifier_dummy_cb,
                           event_notifier_poll);
    return ctx;
fail:
    g_source_destroy(&ctx->source);
    return NULL;
}

void
aio_list_bh(AioContext *ctx) {
    QEMUBH *bh = NULL;
    QEMUBH *next = NULL;

    for (bh = atomic_rcu_read(&ctx->first_bh); bh; bh = next) {
        next = atomic_rcu_read(&bh->next);
        g_print("[%s] bh = %p \n", __FUNCTION__, bh);
    }
}
