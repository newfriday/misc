#include <glib.h>

gboolean io_watch(GIOChannel *channel,
                  GIOCondition condition,
                  gpointer data)
{
    gsize len = 0;
    gchar *buffer = NULL;

    g_io_channel_read_line(channel, &buffer, &len, NULL, NULL);

    if(len > 0)
        g_print("%d\n", len);

    g_free(buffer);

    return TRUE;
}

int main(int argc, char* argv[])
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    GIOChannel* channel = g_io_channel_unix_new(1);

    if(channel) {
        g_io_add_watch(channel, G_IO_IN, io_watch, NULL);
        g_io_channel_unref(channel);
    }

    g_main_loop_run(loop);
    g_main_context_unref(g_main_loop_get_context(loop));
    g_main_loop_unref(loop);

    return 0;
}
