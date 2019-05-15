#include <dbus/dbus.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static DBusHandlerResult
filter_func(DBusConnection *connection, DBusMessage *message, void *usr_data)
{
    dbus_bool_t handled = false;
    char *word = NULL;
    DBusError dberr;

    if (dbus_message_is_signal(message, "client.signal.Type", "Test")) {
        dbus_error_init(&dberr);
        dbus_message_get_args(message, &dberr, DBUS_TYPE_STRING,
            &word, DBUS_TYPE_INVALID);
        if (dbus_error_is_set(&dberr)) {
            dbus_error_free(&dberr);
        } else {
            printf("receive message %s\n", word);
            handled = true;
        }
    }
    return (handled ? DBUS_HANDLER_RESULT_HANDLED : DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
}

int main(int argc, char *argv[])
{
    DBusError dberr;
    DBusConnection *dbconn;

    dbus_error_init(&dberr);

    dbconn = dbus_bus_get(DBUS_BUS_SESSION, &dberr);
    if (dbus_error_is_set(&dberr)) {
        dbus_error_free(&dberr);
        return -1;
    }

    if (!dbus_connection_add_filter(dbconn, filter_func, NULL, NULL)) {
        return -1;
    }

    dbus_bus_add_match(dbconn, "type='signal',interface='client.signal.Type'", &dberr);
    if (dbus_error_is_set(&dberr)) {
        dbus_error_free(&dberr);
        return -1;
    }

    while(dbus_connection_read_write_dispatch(dbconn, -1)) {
        /* loop */
    }
    return 0;
}
