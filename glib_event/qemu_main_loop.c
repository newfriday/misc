#include <glib.h>
#include <unistd.h>

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

typedef void IOHandler(void *opaque);

struct AioContext {
    GSource source;
    QLIST_HEAD(, AioHandler) aio_handlers;
};

struct AioHandler {
    GPollFD pfd;
    IOHandler *io_read;
    IOHandler *io_write;
    void *opaque;
    QLIST_ENTRY(AioHandler) node;
};

typedef struct AioContext AioContext;
typedef struct AioHandler AioHandler;

static AioContext *qemu_aio_context;
static AioContext *iohandler_ctx;
static GArray *gpollfds;
static int max_priority;

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

static gboolean
aio_prepare(AioContext *ctx)
{
    return FALSE;
}

static gboolean
aio_ctx_prepare(GSource *source, gint    *timeout)
{
#ifdef DEBUG
    g_print("%s\n", __FUNCTION__);
#endif
    AioContext *ctx = (AioContext *) source;

    /* default timeout 10ms */
    *timeout = 10;

    if (aio_prepare(ctx)) {
        *timeout = 0;
    }

    return *timeout == 0;
}

static gboolean
aio_pending(AioContext *ctx)
{
    AioHandler *node;
    gboolean result = FALSE;

    QLIST_FOREACH(node, &ctx->aio_handlers, node) {
        int revents;

        revents = node->pfd.revents & node->pfd.events;
        if (revents & (G_IO_IN | G_IO_HUP | G_IO_ERR) && node->io_read) {
            result = TRUE;
            break;
        }
        if (revents & (G_IO_OUT | G_IO_ERR) && node->io_write) {
            result = TRUE;
            break;
        }
    }

    return result;
}

static gboolean
aio_ctx_check(GSource *source)
{
#ifdef DEBUG
    g_print("%s\n", __FUNCTION__);
#endif
    AioContext *ctx = (AioContext *) source;

    return aio_pending(ctx);
}

static gboolean
aio_dispatch_handlers(AioContext *ctx)
{
    AioHandler *node, *tmp;

    QLIST_FOREACH(node, &ctx->aio_handlers, node) {
        int revents;

        revents = node->pfd.revents & node->pfd.events;
        node->pfd.revents = 0;

        if ((revents & (G_IO_IN | G_IO_HUP | G_IO_ERR)) &&
            node->io_read) {
            node->io_read(node->opaque);
        }
        if ((revents & (G_IO_OUT | G_IO_ERR)) &&
            node->io_write) {
            node->io_write(node->opaque);
        }
    }

    return TRUE;
}

static void
aio_dispatch(AioContext *ctx)
{
    aio_dispatch_handlers(ctx);
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
}

static GSourceFuncs
aio_source_funcs = {
    aio_ctx_prepare,
    aio_ctx_check,
    aio_ctx_dispatch,
    aio_ctx_finalize
};

AioContext *
aio_context_new()
{
    AioContext *ctx;

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    ctx = (AioContext *) g_source_new(&aio_source_funcs, sizeof(AioContext));

    return ctx;
}

static void
aio_remove_fd_handler(AioContext *ctx, AioHandler *node)
{
    /* If the GSource is in the process of being destroyed then
     * g_source_remove_poll() causes an assertion failure.  Skip
     * removal in that case, because glib cleans up its state during
     * destruction anyway.
     */
    if (!g_source_is_destroyed(&ctx->source)) {
        g_source_remove_poll(&ctx->source, &node->pfd);
    }

    /* Otherwise, delete it for real.  We can't just mark it as
     * deleted because deleted nodes are only cleaned up while
     * no one is walking the handlers list.
     */
    QLIST_REMOVE(node, node);
    g_free(node);
}

static AioHandler *
find_aio_handler(AioContext *ctx, int fd)
{
    AioHandler *node;

    QLIST_FOREACH(node, &ctx->aio_handlers, node) {
        if (node->pfd.fd == fd)
            return node;
    }

    return NULL;
}

static void
aio_set_fd_handler(AioContext *ctx,
                   int fd,
                   IOHandler *io_read,
                   IOHandler *io_write,
                   void *opaque)
{
    AioHandler *node;
    AioHandler *new_node = NULL;
    gboolean is_new = FALSE;

    node = find_aio_handler(ctx, fd);

    /* Are we deleting the fd handler? */
    if (!io_read && !io_write) {
        if (node == NULL) {
            return;
        }
        /* Clean events in order to unregister fd from the ctx epoll. */
        node->pfd.events = 0;
    } else {
        if (node == NULL) {
            is_new = TRUE;
        }
        /* Alloc and insert if it's not already there */
        new_node = g_new0(AioHandler, 1);

        /* Update handler with latest information */
        new_node->io_read = io_read;
        new_node->io_write = io_write;
        new_node->opaque = opaque;

        if (is_new) {
            new_node->pfd.fd = fd;
        } else {
            new_node->pfd = node->pfd;
        }
        g_source_add_poll(&ctx->source, &new_node->pfd);

        new_node->pfd.events = (io_read ? G_IO_IN | G_IO_HUP | G_IO_ERR : 0);
        new_node->pfd.events |= (io_write ? G_IO_OUT | G_IO_ERR : 0);

        QLIST_INSERT_HEAD(&ctx->aio_handlers, new_node, node);
    }

    if (node) {
        aio_remove_fd_handler(ctx, node);
    }
}

static void
qemu_set_fd_handler(int fd,
                    IOHandler *fd_read,
                    IOHandler *fd_write,
                    void *opaque)
{
    aio_set_fd_handler(qemu_aio_context, fd,
                       fd_read, fd_write,
                       opaque);
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

static void main_loop()
{
    while (TRUE) {
        main_loop_wait();
    }
}

int main(int argc, char* argv[])
{
    int fd;
    GError *error = NULL;
    GIOChannel *channel;

    qemu_init_main_loop();

    if (!(channel = g_io_channel_new_file("test", "r", &error))) {
        if (error != NULL)
            g_print("Unable to get test file channel: %s\n", error->message);

        return -1;
    }

    fd = g_io_channel_unix_get_fd(channel);
    qemu_set_fd_handler(fd, fd_read_cb, NULL, channel);

    main_loop();

    return 0;
}
