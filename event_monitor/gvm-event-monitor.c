/* compile: gcc -lvirt glib-2.0 gvm-event-monitor.c util/misc.c -o gvm-event-monitor */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <glib.h>
#include <errno.h>
#include <inttypes.h>

#define VIR_ENUM_SENTINELS

#include <libvirt/libvirt.h>
#include <libvirt/libvirt-event.h>
#include <libvirt/virterror.h>

#include "gvm-event-monitor.h"
#include "misc.h"

#define STREQ(a, b) (strcmp(a, b) == 0)
#define NULLSTR(s) ((s) ? (s) : "<null>")

int run = 1;

/* Callback functions */
static void
gemConnectClose(virConnectPtr conn,
                int reason,
                void *opaque)
{
    run = 0;

    switch ((virConnectCloseReason) reason) {
    case VIR_CONNECT_CLOSE_REASON_ERROR:
        fprintf(stderr, "Connection closed due to I/O error\n");
        return;

    case VIR_CONNECT_CLOSE_REASON_EOF:
        fprintf(stderr, "Connection closed due to end of file\n");
        return;

    case VIR_CONNECT_CLOSE_REASON_KEEPALIVE:
        fprintf(stderr, "Connection closed due to keepalive timeout\n");
        return;

    case VIR_CONNECT_CLOSE_REASON_CLIENT:
        fprintf(stderr, "Connection closed due to client request\n");
        return;

    case VIR_CONNECT_CLOSE_REASON_LAST:
        break;
    };

    fprintf(stderr, "Connection closed due to unknown reason\n");
}

static const char *
gemEventToString(int event)
{
    switch ((virDomainEventType) event) {
        case VIR_DOMAIN_EVENT_DEFINED:
            return "Defined";

        case VIR_DOMAIN_EVENT_UNDEFINED:
            return "Undefined";

        case VIR_DOMAIN_EVENT_STARTED:
            return "Started";

        case VIR_DOMAIN_EVENT_SUSPENDED:
            return "Suspended";

        case VIR_DOMAIN_EVENT_RESUMED:
            return "Resumed";

        case VIR_DOMAIN_EVENT_STOPPED:
            return "Stopped";

        case VIR_DOMAIN_EVENT_SHUTDOWN:
            return "Shutdown";

        case VIR_DOMAIN_EVENT_PMSUSPENDED:
            return "PMSuspended";

        case VIR_DOMAIN_EVENT_CRASHED:
            return "Crashed";

        case VIR_DOMAIN_EVENT_LAST:
            break;
    }

    return "unknown";
}

static const char *
gemEventDetailToString(int event,
                    int detail)
{
    switch ((virDomainEventType) event) {
        case VIR_DOMAIN_EVENT_DEFINED:
            switch ((virDomainEventDefinedDetailType) detail) {
            case VIR_DOMAIN_EVENT_DEFINED_ADDED:
                return "Added";

            case VIR_DOMAIN_EVENT_DEFINED_UPDATED:
                return "Updated";

            case VIR_DOMAIN_EVENT_DEFINED_RENAMED:
                return "Renamed";

            case  VIR_DOMAIN_EVENT_DEFINED_FROM_SNAPSHOT:
                return "Snapshot";

            case VIR_DOMAIN_EVENT_DEFINED_LAST:
                break;
            }
            break;

        case VIR_DOMAIN_EVENT_UNDEFINED:
            switch ((virDomainEventUndefinedDetailType) detail) {
            case VIR_DOMAIN_EVENT_UNDEFINED_REMOVED:
                return "Removed";

            case VIR_DOMAIN_EVENT_UNDEFINED_RENAMED:
                return "Renamed";

            case VIR_DOMAIN_EVENT_UNDEFINED_LAST:
                break;
            }
            break;

        case VIR_DOMAIN_EVENT_STARTED:
            switch ((virDomainEventStartedDetailType) detail) {
            case VIR_DOMAIN_EVENT_STARTED_BOOTED:
                return "Booted";

            case VIR_DOMAIN_EVENT_STARTED_MIGRATED:
                return "Migrated";

            case VIR_DOMAIN_EVENT_STARTED_RESTORED:
                return "Restored";

            case VIR_DOMAIN_EVENT_STARTED_FROM_SNAPSHOT:
                return "Snapshot";

            case VIR_DOMAIN_EVENT_STARTED_WAKEUP:
                return "Event wakeup";

            case VIR_DOMAIN_EVENT_STARTED_LAST:
                break;
            }
            break;

        case VIR_DOMAIN_EVENT_SUSPENDED:
            switch ((virDomainEventSuspendedDetailType) detail) {
            case VIR_DOMAIN_EVENT_SUSPENDED_PAUSED:
                return "Paused";

            case VIR_DOMAIN_EVENT_SUSPENDED_MIGRATED:
                return "Migrated";

            case VIR_DOMAIN_EVENT_SUSPENDED_IOERROR:
                return "I/O Error";

            case VIR_DOMAIN_EVENT_SUSPENDED_WATCHDOG:
                return "Watchdog";

            case VIR_DOMAIN_EVENT_SUSPENDED_RESTORED:
                return "Restored";

            case VIR_DOMAIN_EVENT_SUSPENDED_FROM_SNAPSHOT:
                return "Snapshot";

            case VIR_DOMAIN_EVENT_SUSPENDED_API_ERROR:
                return "API error";

            case VIR_DOMAIN_EVENT_SUSPENDED_POSTCOPY:
                return "Post-copy";

            case VIR_DOMAIN_EVENT_SUSPENDED_POSTCOPY_FAILED:
                return "Post-copy Error";

            case VIR_DOMAIN_EVENT_SUSPENDED_LAST:
                break;
            }
            break;

        case VIR_DOMAIN_EVENT_RESUMED:
            switch ((virDomainEventResumedDetailType) detail) {
            case VIR_DOMAIN_EVENT_RESUMED_UNPAUSED:
                return "Unpaused";

            case VIR_DOMAIN_EVENT_RESUMED_MIGRATED:
                return "Migrated";

            case VIR_DOMAIN_EVENT_RESUMED_FROM_SNAPSHOT:
                return "Snapshot";

            case VIR_DOMAIN_EVENT_RESUMED_POSTCOPY:
                return "Post-copy";

            case VIR_DOMAIN_EVENT_RESUMED_LAST:
                break;
            }
            break;

        case VIR_DOMAIN_EVENT_STOPPED:
            switch ((virDomainEventStoppedDetailType) detail) {
            case VIR_DOMAIN_EVENT_STOPPED_SHUTDOWN:
                return "Shutdown";

            case VIR_DOMAIN_EVENT_STOPPED_DESTROYED:
                return "Destroyed";

            case VIR_DOMAIN_EVENT_STOPPED_CRASHED:
                return "Crashed";

            case VIR_DOMAIN_EVENT_STOPPED_MIGRATED:
                return "Migrated";

            case VIR_DOMAIN_EVENT_STOPPED_SAVED:
                return "Saved";

            case VIR_DOMAIN_EVENT_STOPPED_FAILED:
                return "Failed";

            case VIR_DOMAIN_EVENT_STOPPED_FROM_SNAPSHOT:
                return "Snapshot";

            case VIR_DOMAIN_EVENT_STOPPED_LAST:
                break;
            }
            break;

        case VIR_DOMAIN_EVENT_SHUTDOWN:
            switch ((virDomainEventShutdownDetailType) detail) {
            case VIR_DOMAIN_EVENT_SHUTDOWN_FINISHED:
                return "Finished";

            case VIR_DOMAIN_EVENT_SHUTDOWN_GUEST:
                return "Guest request";

            case VIR_DOMAIN_EVENT_SHUTDOWN_HOST:
                return "Host request";

            case VIR_DOMAIN_EVENT_SHUTDOWN_LAST:
                break;
            }
            break;

        case VIR_DOMAIN_EVENT_PMSUSPENDED:
            switch ((virDomainEventPMSuspendedDetailType) detail) {
            case VIR_DOMAIN_EVENT_PMSUSPENDED_MEMORY:
                return "Memory";

            case VIR_DOMAIN_EVENT_PMSUSPENDED_DISK:
                return "Disk";

            case VIR_DOMAIN_EVENT_PMSUSPENDED_LAST:
                break;
            }
            break;

        case VIR_DOMAIN_EVENT_CRASHED:
           switch ((virDomainEventCrashedDetailType) detail) {
           case VIR_DOMAIN_EVENT_CRASHED_PANICKED:
               return "Panicked";
#if (LIBVIR_VERSION_NUMBER > 6000000)
           case VIR_DOMAIN_EVENT_CRASHED_CRASHLOADED:
               return "Crashloaded";
#endif
           case VIR_DOMAIN_EVENT_CRASHED_LAST:
               break;
           }
           break;

        case VIR_DOMAIN_EVENT_LAST:
           break;
    }

    return "unknown";
}

static int
gemDomainEventLifeCycleCallback(virConnectPtr conn,
                                virDomainPtr dom,
                                int event,
                                int detail,
                                void *opaque)
{
    fprintf(stderr, "%s EVENT: Domain %s(%d) %s %s\n", __func__, virDomainGetName(dom),
           virDomainGetID(dom), gemEventToString(event),
           gemEventDetailToString(event, detail));

    return 0;
}

static void
gemFreeFunc(void *opaque)
{
    char *str = opaque;
    printf("%s: Freeing [%s]\n", __func__, str);
    free(str);
}

/* main test functions */
static void
gemStop(int sig)
{
    printf("Exiting on signal %d\n", sig);
    run = 0;
}

struct domainEventData {
    int event;
    int id;
    virConnectDomainEventGenericCallback cb;
    const char *name;
};

#define DOMAIN_EVENT(event, callback) \
    {event, -1, VIR_DOMAIN_EVENT_CALLBACK(callback), #event}

struct domainEventData domainEvents[] = {
    DOMAIN_EVENT(VIR_DOMAIN_EVENT_ID_LIFECYCLE, gemDomainEventLifeCycleCallback),
};

int
main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    virConnectPtr dconn = NULL;
    size_t i;

    if (argc > 1 && STREQ(argv[1], "--help")) {
        printf("%s uri\n", argv[0]);
        goto cleanup;
    }

    if (virInitialize() < 0) {
        fprintf(stderr, "Failed to initialize libvirt");
        goto cleanup;
    }

    if (virEventRegisterDefaultImpl() < 0) {
        fprintf(stderr, "Failed to register event implementation: %s\n",
                virGetLastErrorMessage());
        goto cleanup;
    }

    dconn = virConnectOpenAuth(argc > 1 ? argv[1] : NULL,
                               virConnectAuthPtrDefault,
                               0);
    if (!dconn) {
        printf("error opening\n");
        goto cleanup;
    }

    if (virConnectRegisterCloseCallback(dconn,
                                        gemConnectClose, NULL, NULL) < 0) {
        fprintf(stderr, "Unable to register close callback\n");
        goto cleanup;
    }

    /* The ideal program would use sigaction to set this handler, but
     * this way is portable to mingw. */
    signal(SIGTERM, gemStop);
    signal(SIGINT, gemStop);

    printf("Registering domain event callbacks\n");

    /* register common domain callbacks */
    for (i = 0; i < G_N_ELEMENTS(domainEvents); i++) {
        struct domainEventData *event = domainEvents + i;

        event->id = virConnectDomainEventRegisterAny(dconn, NULL,
                                                     event->event,
                                                     event->cb,
                                                     strdup(event->name),
                                                     gemFreeFunc);

        if (event->id < 0) {
            fprintf(stderr, "Failed to register event '%s'\n", event->name);
            goto cleanup;
        }
    }

    if (virConnectSetKeepAlive(dconn, 5, 3) < 0) {
        fprintf(stderr, "Failed to start keepalive protocol: %s\n",
                virGetLastErrorMessage());
        run = 0;
    }

    while (run) {
        if (virEventRunDefaultImpl() < 0) {
            fprintf(stderr, "Failed to run event loop: %s\n",
                    virGetLastErrorMessage());
        }
    }

    printf("Deregistering domain event callbacks\n");
    for (i = 0; i < G_N_ELEMENTS(domainEvents); i++) {
        if (domainEvents[i].id > 0)
            virConnectDomainEventDeregisterAny(dconn, domainEvents[i].id);
    }

    virConnectUnregisterCloseCallback(dconn, gemConnectClose);
    ret = EXIT_SUCCESS;

 cleanup:
    if (dconn) {
        printf("Closing connection: ");
        if (virConnectClose(dconn) < 0)
            printf("failed\n");
        printf("done\n");
    }

    return ret;
}
