#include <glib.h>

typedef struct _MySource MySource;

struct _MySource
{
    GSource _source;
    GIOChannel *channel;
    GPollFD fd;
};

static gboolean watch(GIOChannel *channel)
{
    gsize len = 0;
    gchar *buffer = NULL;

    g_io_channel_read_line(channel, &buffer, &len, NULL, NULL);
    if(len > 0)
      g_print("%d\n", len);
    g_free(buffer);

    return TRUE;
}

static gboolean prepare(GSource *source, gint *timeout)
{
    *timeout = -1;
    return FALSE;
}

static gboolean check(GSource *source)
{
    MySource *mysource = (MySource *)source;

    if(mysource->fd.revents != mysource->fd.events)
      return FALSE;

    return TRUE;
}

static gboolean dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
    MySource *mysource = (MySource *)source;

    if(callback)
      callback(mysource->channel);

    return TRUE;
}

static void finalize(GSource *source)
{
    MySource *mysource = (MySource *)source;

    if(mysource->channel)
      g_io_channel_unref(mysource->channel);
}

int main(int argc, char* argv[])
{
    GError *error = NULL;
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    GSourceFuncs funcs = {prepare, check, dispatch, finalize};
    GSource *source = g_source_new(&funcs, sizeof(MySource));
    MySource *mysource = (MySource *)source;

    if (!(mysource->channel = g_io_channel_new_file("test", "r", &error))) {
        if (error != NULL)
            g_print("Unable to get test file channel: %s\n", error->message);

        return -1;
    }

    mysource->fd.fd = g_io_channel_unix_get_fd(mysource->channel);
    mysource->fd.events = G_IO_IN;
    g_source_add_poll(source, &mysource->fd);
    g_source_set_callback(source, (GSourceFunc)watch, NULL, NULL);
    g_source_set_priority(source, G_PRIORITY_DEFAULT_IDLE);
    g_source_attach(source, NULL);
    g_source_unref(source);

    g_main_loop_run(loop);

    g_main_context_unref(g_main_loop_get_context(loop));
    g_main_loop_unref(loop);

    return 0;
}
