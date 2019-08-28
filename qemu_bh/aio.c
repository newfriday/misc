#include <glib.h>
#include "aio.h"

gboolean
aio_prepare(AioContext *ctx)
{
    return FALSE;
}

gboolean
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

gboolean
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

static bool aio_remove_fd_handler(AioContext *ctx, AioHandler *node)
{
    /* If the GSource is in the process of being destroyed then
     * g_source_remove_poll() causes an assertion failure.  Skip
     * removal in that case, because glib cleans up its state during
     * destruction anyway.
     */
    if (!g_source_is_destroyed(&ctx->source)) {
        g_source_remove_poll(&ctx->source, &node->pfd);
    }

    /* If a read is in progress, just mark the node as deleted */
    if (qemu_lockcnt_count(&ctx->list_lock)) {
        node->deleted = 1;
        node->pfd.revents = 0;
        return false;
    }
    /* Otherwise, delete it for real.  We can't just mark it as
     * deleted because deleted nodes are only cleaned up while
     * no one is walking the handlers list.
     */
    QLIST_REMOVE(node, node);
    return true;
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


void aio_set_fd_handler(AioContext *ctx,
                        int fd,
                        IOHandler *io_read,
                        IOHandler *io_write,
                        AioPollFn *io_poll,
                        void *opaque)
{
    AioHandler *node;
    AioHandler *new_node = NULL;
    bool is_new = FALSE;
    bool deleted = false;
    int poll_disable_change;

    qemu_lockcnt_lock(&ctx->list_lock);

    node = find_aio_handler(ctx, fd);

    /* Are we deleting the fd handler? */
    if (!io_read && !io_write && !io_poll) {
        if (node == NULL) {
            return;
        }
        /* Clean events in order to unregister fd from the ctx epoll. */
        node->pfd.events = 0;
        poll_disable_change = -!node->io_poll;
    } else {
        poll_disable_change = !io_poll - (node && !node->io_poll);
        if (node == NULL) {
            is_new = TRUE;
        }
        /* Alloc and insert if it's not already there */
        new_node = g_new0(AioHandler, 1);

        /* Update handler with latest information */
        new_node->io_read = io_read;
        new_node->io_write = io_write;
        new_node->io_poll = io_poll;
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
        deleted = aio_remove_fd_handler(ctx, node);
    }

    qemu_lockcnt_unlock(&ctx->list_lock);
    aio_notify(ctx);

    if (deleted) {
        g_free(node);
    }
}


void aio_set_event_notifier(AioContext *ctx,
                            EventNotifier *notifier,
                            EventNotifierHandler *io_read,
                            AioPollFn *io_poll)
{
    aio_set_fd_handler(ctx, event_notifier_get_fd(notifier),
                       (IOHandler *)io_read, NULL, io_poll, notifier);
}
