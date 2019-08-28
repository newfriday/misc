#include <glib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include "async.h"

static void
fd_read_cb(void *opaque)
{
    GIOChannel *channel = opaque;
    gsize len = 0;
    gchar *buffer = NULL;

    g_io_channel_read_line(channel, &buffer, &len, NULL, NULL);
    if(len > 0) {
        g_print("len: %d, buffer: %s\n", len, buffer);
    }
    g_free(buffer);
}

static void
qemu_set_fd_handler(int fd,
                    IOHandler *fd_read,
                    IOHandler *fd_write,
                    void *opaque)
{
    aio_set_fd_handler(qemu_aio_context, fd,
                       fd_read, fd_write,
                       NULL, opaque);
}

static void
iohandler_init(void)
{
    if (!iohandler_ctx) {
        iohandler_ctx = aio_context_new();
    }
}

static GSource *
aio_get_g_source(AioContext *ctx)
{
    g_source_ref(&ctx->source);
    return &ctx->source;
}

static GSource *
iohandler_get_g_source(void)
{
    iohandler_init();
    return aio_get_g_source(iohandler_ctx);
}

static void
qemu_init_main_loop()
{
    GSource *src;

    qemu_aio_context = aio_context_new();
    if (!qemu_aio_context) {
        perror("create main aio context\n");
        return ;
    }

    gpollfds = g_array_new(FALSE, FALSE, sizeof(GPollFD));

    src = aio_get_g_source(qemu_aio_context);
    g_source_set_name(src, "aio-context");
    g_source_attach(src, NULL);
    g_source_unref(src);

    src = iohandler_get_g_source();
    g_source_set_name(src, "io-handler");
    g_source_attach(src, NULL);
    g_source_unref(src);
}

static int glib_pollfds_idx;
static int glib_n_poll_fds;

static void glib_pollfds_fill()
{
    GMainContext *context = g_main_context_default();
    int n;

    g_main_context_prepare(context, &max_priority);

    glib_pollfds_idx = gpollfds->len;
    n = glib_n_poll_fds;

    do {
        GPollFD *pfds;
        glib_n_poll_fds = n;
        g_array_set_size(gpollfds, glib_pollfds_idx + glib_n_poll_fds);
        pfds = &g_array_index(gpollfds, GPollFD, glib_pollfds_idx);
        n = g_main_context_query(context, max_priority, NULL, pfds,
                                 glib_n_poll_fds);
    } while (n != glib_n_poll_fds);
}

static int qemu_poll_ns(GPollFD *fds, guint nfds)
{
    return ppoll((struct pollfd *)fds, nfds, NULL, NULL);
}

static void glib_pollfds_poll(void)
{
    GMainContext *context = g_main_context_default();
    GPollFD *pfds = &g_array_index(gpollfds, GPollFD, glib_pollfds_idx);

    if (g_main_context_check(context, max_priority, pfds, glib_n_poll_fds)) {
        g_main_context_dispatch(context);
    }
}

static int main_loop_wait()
{
    GMainContext *context = g_main_context_default();
    int ret;

    g_main_context_acquire(context);

    g_array_set_size(gpollfds, 0);
    glib_pollfds_fill();

    ret = qemu_poll_ns((GPollFD *)gpollfds->data, gpollfds->len);

    glib_pollfds_poll();

    g_main_context_release(context);

    return ret;
}

static AioContext *
qemu_get_aio_context(void)
{
    return qemu_aio_context;
}

static void main_loop()
{
    while (TRUE) {
        main_loop_wait();
    }
}

/* Functions to operate on the main QEMU AioContext.  */
static QEMUBH *qemu_bh_new(QEMUBHFunc *cb, void *opaque)
{
    return aio_bh_new(qemu_aio_context, cb, opaque);
}

static void qemu_test_cb(void *opaque)
{
    printf("[%s] executing \n", __FUNCTION__);
}

static void qemu_test_cb_oneshot(void *opaque)
{
    printf("[%s] executing \n", __FUNCTION__);
}

void *thread_context()
{
    int i = 0;
    QEMUBH *qemu_test_bh;
    qemu_test_bh = qemu_bh_new(qemu_test_cb, NULL);
    AioContext *main_ctx = qemu_get_aio_context();

    while (i++ < 4){
        qemu_bh_schedule(qemu_test_bh);

        aio_list_bh(main_ctx);

        aio_bh_schedule_oneshot(main_ctx,
                                qemu_test_cb_oneshot,
                                NULL);

        sleep(2);

        aio_list_bh(main_ctx);

        printf("[%s] loop count = %d \n\n", __FUNCTION__, i);
    }//end while
}

int main(int argc, char* argv[])
{
    int fd;
    GError *error = NULL;
    pthread_t tid;

    qemu_init_main_loop();

    if(pthread_create(&tid, NULL,
                      thread_context,
                      NULL) < 0) {
        perror("pthread_create");
        return -errno;
    }

    main_loop();

    return 0;
}
