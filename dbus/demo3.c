#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dbus/dbus.h>
#include <gnulib/lib/xalloc-oversized.h>

/*
 * listen, wait a call or a signal
 */
#define DBUS_SENDER_BUS_NAME        "com.ty3219.sender_app"

#define DBUS_RECEIVER_BUS_NAME      "com.ty3219.receiver_app"
#define DBUS_RECEIVER_PATH          "/com/ty3219/object"
#define DBUS_RECEIVER_INTERFACE     "com.ty3219.interface"
#define DBUS_RECEIVER_SIGNAL        "signal"
#define DBUS_RECEIVER_METHOD        "method"

#define DBUS_RECEIVER_SIGNAL_RULE   "type='signal',interface='%s'"
#define DBUS_RECEIVER_REPLY_STR     "i am %d, get a message"

#define MODE_SIGNAL                 1
#define MODE_METHOD                 2

#define DBUS_CLIENT_PID_FILE        "/tmp/dbus-client.pid"

# define VIR_DBUS_TYPE_STACK_MAX_DEPTH 32

# define ARRAY_CARDINALITY(Array) (sizeof(Array) / sizeof(*(Array)))

# define VIR_DBUS_METHOD_CALL_TIMEOUT_MILLIS 30 * 1000

# define SET_NEXT_VAL(dbustype, vargtype, arrtype, sigtype, fmt) \
    do { \
        dbustype x; \
        if (arrayref) { \
            arrtype valarray = arrayptr; \
            x = (dbustype)*valarray; \
            valarray++; \
            arrayptr = valarray; \
        } else { \
            x = (dbustype)va_arg(args, vargtype); \
        } \
        if (!dbus_message_iter_append_basic(iter, sigtype, &x)) { \
            printf("Cannot append basic type %s\n", #vargtype);\
            goto cleanup; \
        } \
        printf("Appended basic type '" #dbustype "' varg '" #vargtype\
                  "' sig '%c' val '" fmt "'", sigtype, (vargtype)x); \
    } while (0)

# define VIR_ALLOC(ptr) virAlloc(&(ptr), sizeof(*(ptr)))
# define VIR_ALLOC_N(ptr, count) virAllocN(&(ptr), sizeof(*(ptr)), (count))

void virFree(void *ptrptr)
{
    int save_errno = errno;

    free(*(void**)ptrptr);
    *(void**)ptrptr = NULL;
    errno = save_errno;
}

static int virReallocN(void *ptrptr,
                size_t size,
                size_t count)
{
    void *tmp;

    if (xalloc_oversized(count, size)) {
        return -1;
    }
    tmp = realloc(*(void**)ptrptr, size * count);
    if (!tmp && ((size * count) != 0)) {
        printf("alloc error\n");
        return -1;
    }
    *(void**)ptrptr = tmp;
    return 0;
}

static int virExpandN(void *ptrptr,
               size_t size,
               size_t *countptr,
               size_t add)
{
    int ret;

    if (*countptr + add < *countptr) {
        return -1;
    }
    ret = virReallocN(ptrptr, size, *countptr + add);
    if (ret == 0) {
        memset(*(char **)ptrptr + (size * *countptr), 0, size * add);
        *countptr += add;
    }
    return ret;
}

# define VIR_EXPAND_N(ptr, count, add) \
    virExpandN(&(ptr), sizeof(*(ptr)), &(count), add)

void virShrinkN(void *ptrptr, size_t size, size_t *countptr, size_t toremove)
{
    if (toremove < *countptr) {
        virReallocN(ptrptr, size, *countptr -= toremove);
    } else {
        virFree(ptrptr);
        *countptr = 0;
    }
}

# define VIR_SHRINK_N(ptr, count, remove) \
    virShrinkN(&(ptr), sizeof(*(ptr)), &(count), remove)

static const char virDBusBasicTypes[] = {
    DBUS_TYPE_BYTE,
    DBUS_TYPE_BOOLEAN,
    DBUS_TYPE_INT16,
    DBUS_TYPE_UINT16,
    DBUS_TYPE_INT32,
    DBUS_TYPE_UINT32,
    DBUS_TYPE_INT64,
    DBUS_TYPE_UINT64,
    DBUS_TYPE_DOUBLE,
    DBUS_TYPE_STRING,
    DBUS_TYPE_OBJECT_PATH,
    DBUS_TYPE_SIGNATURE,
};

static int virDBusIsBasicType(char c)
{
    int ret = 0;
    if (memchr(virDBusBasicTypes, c, ARRAY_CARDINALITY(virDBusBasicTypes)))
        ret = 1;

    return ret;
}

void virDBusMessageUnref(DBusMessage *msg)
{
    if (msg)
        dbus_message_unref(msg);
}

static int
virDBusIsAllowedRefType(const char *sig)
{
    if (*sig == '{') {
        if (strlen(sig) != 4)
            return 0;
        if (!virDBusIsBasicType(sig[1]) ||
            !virDBusIsBasicType(sig[2]) ||
            sig[1] != sig[2])
            return 0;
        if (sig[3] != '}')
            return 0;
    } else {
        if (strlen(sig) != 1)
            return 0;
        if (!virDBusIsBasicType(sig[0]))
            return 0;
    }
    return 1;
}

int virAlloc(void *ptrptr,
             size_t size)
{
    *(void **)ptrptr = calloc(1, size);
    if (*(void **)ptrptr == NULL) {
        printf("calloc error\n");
        return -1;
    }
    return 0;
}

int virAllocN(void *ptrptr,
              size_t size,
              size_t count)
{
    *(void**)ptrptr = calloc(count, size);
    if (*(void**)ptrptr == NULL) {
        printf("calloc error\n");
        return -1;
    }
    return 0;
}


static int
virDBusSignatureLengthInternal(const char *s,
                               int allowDict,
                               unsigned arrayDepth,
                               unsigned structDepth,
                               size_t *skiplen,
                               size_t *siglen)
{
    if (virDBusIsBasicType(*s) || *s == DBUS_TYPE_VARIANT) {
        *skiplen = *siglen = 1;
        return 0;
    }

    if (*s == DBUS_TYPE_ARRAY) {
        size_t skiplencont;
        size_t siglencont;
        int arrayref = 0;

        if (arrayDepth >= VIR_DBUS_TYPE_STACK_MAX_DEPTH) {
            printf("Signature '%s' too deeply nested\n",s);
            return -1;
        }

        if (*(s + 1) == '&') {
            arrayref = 1;
            s++;
        }

        if (virDBusSignatureLengthInternal(s + 1,
                                           1,
                                           arrayDepth + 1,
                                           structDepth,
                                           &skiplencont,
                                           &siglencont) < 0)
            return -1;

        *skiplen = skiplencont + 1;
        *siglen = siglencont + 1;
        if (arrayref)
            (*skiplen)++;
        return 0;
    }

    if (*s == DBUS_STRUCT_BEGIN_CHAR) {
        const char *p = s + 1;

        if (structDepth >= VIR_DBUS_TYPE_STACK_MAX_DEPTH) {
            printf("Signature '%s' too deeply nested\n", s);
            return -1;
        }

        *skiplen = *siglen = 2;

        while (*p != DBUS_STRUCT_END_CHAR) {
            size_t skiplencont;
            size_t siglencont;

            if (virDBusSignatureLengthInternal(p,
                                               0,
                                               arrayDepth,
                                               structDepth + 1,
                                               &skiplencont,
                                               &siglencont) < 0)
                return -1;

            p += skiplencont;
            *skiplen += skiplencont;
            *siglen += siglencont;
        }

        return 0;
    }

    if (*s == DBUS_DICT_ENTRY_BEGIN_CHAR && allowDict) {
        const char *p = s + 1;
        unsigned n = 0;
        if (structDepth >= VIR_DBUS_TYPE_STACK_MAX_DEPTH) {
            printf("Signature '%s' too deeply nested\n", s);
            return -1;
        }

        *skiplen = *siglen = 2;

        while (*p != DBUS_DICT_ENTRY_END_CHAR) {
            size_t skiplencont;
            size_t siglencont;

            if (n == 0 && !virDBusIsBasicType(*p)) {
                printf("Dict entry in signature '%s' must be a basic type\n", s);
                return -1;
            }

            if (virDBusSignatureLengthInternal(p,
                                               0,
                                               arrayDepth,
                                               structDepth + 1,
                                               &skiplencont,
                                               &siglencont) < 0)
                return -1;

            p += skiplencont;
            *skiplen += skiplencont;
            *siglen += siglencont;
            n++;
        }

        if (n != 2) {
            printf("Dict entry in signature '%s' is wrong size\n", s);
            return -1;
        }

        return 0;
    }

    printf("Unexpected signature '%s'\n", s);
    return -1;
}


static int virDBusSignatureLength(const char *s, size_t *skiplen, size_t *siglen)
{
    return virDBusSignatureLengthInternal(s, 1, 0, 0, skiplen, siglen);
}


static char *virDBusCopyContainerSignature(const char *sig,
                                           size_t *skiplen,
                                           size_t *siglen)
{
    size_t i, j;
    char *contsig;
    int isGroup;

    isGroup = (sig[0] == DBUS_STRUCT_BEGIN_CHAR ||
               sig[0] == DBUS_DICT_ENTRY_BEGIN_CHAR);

    if (virDBusSignatureLength(isGroup ? sig : sig + 1, skiplen, siglen) < 0)
        return NULL;

    if (VIR_ALLOC_N(contsig, *siglen + 1) < 0)
        return NULL;

    for (i = 0, j = 0; i < *skiplen && j < *siglen; i++) {
        if (sig[i + 1] == '&')
            continue;
        contsig[j] = sig[i + 1];
        j++;
    }
    contsig[*siglen] = '\0';
    printf("Extracted '%s' from '%s'\n", contsig, sig);
    return contsig;
}


typedef struct _virDBusTypeStack virDBusTypeStack;
struct _virDBusTypeStack {
    const char *types;
    size_t nstruct;
    size_t narray;
    DBusMessageIter *iter;
};

static int virDBusTypeStackPop(virDBusTypeStack **stack,
                               size_t *nstack,
                               DBusMessageIter **iter,
                               const char **types,
                               size_t *nstruct,
                               size_t *narray)
{
    if (*nstack == 0) {
        printf("DBus type stack is empty\n");
        return -1;
    }

    *iter = (*stack)[(*nstack) - 1].iter;
    *types = (*stack)[(*nstack) - 1].types;
    *nstruct = (*stack)[(*nstack) - 1].nstruct;
    *narray = (*stack)[(*nstack) - 1].narray;
    printf("Popped types='%s' nstruct=%zu narray=%zd\n",
           *types, *nstruct, (ssize_t)*narray);
    VIR_SHRINK_N(*stack, *nstack, 1);

    return 0;
}


static int virDBusTypeStackPush(virDBusTypeStack **stack,
                                size_t *nstack,
                                DBusMessageIter *iter,
                                const char *types,
                                size_t nstruct,
                                size_t narray)
{
    if (*nstack >= VIR_DBUS_TYPE_STACK_MAX_DEPTH) {
        printf("DBus type too deeply nested\n");
        return -1;
    }

    if (VIR_EXPAND_N(*stack, *nstack, 1) < 0)
        return -1;

    (*stack)[(*nstack) - 1].iter = iter;
    (*stack)[(*nstack) - 1].types = types;
    (*stack)[(*nstack) - 1].nstruct = nstruct;
    (*stack)[(*nstack) - 1].narray = narray;
    printf("Pushed types='%s' nstruct=%zu narray=%zd\n",
           types, nstruct, (ssize_t)narray);
    return 0;
}

static int
virDBusMessageIterEncode(DBusMessageIter *rootiter,
                         const char *types,
                         va_list args)
{
    int ret = -1;
    size_t narray;
    size_t nstruct;
    int arrayref = 0;
    void *arrayptr = NULL;
    virDBusTypeStack *stack = NULL;
    size_t nstack = 0;
    size_t siglen;
    size_t skiplen;
    char *contsig = NULL;
    const char *vsig;
    DBusMessageIter *newiter = NULL;
    DBusMessageIter *iter = rootiter;

    printf("rootiter=%p types=%s\n", rootiter, types);

    if (!types)
        return 0;

    narray = (size_t)-1;
    nstruct = strlen(types);

    for (;;) {
        const char *t;

        printf("Loop nstack=%zu narray=%zd nstruct=%zu types='%s'\n",
                  nstack, (ssize_t)narray, nstruct, types);
        if (narray == 0 ||
            (narray == (size_t)-1 &&
             nstruct == 0)) {
            DBusMessageIter *thisiter = iter;
            if (*types != '}') {
                printf("Reset array ref\n");
                arrayref = 0;
                arrayptr = NULL;
            }
            printf("Popping iter=%p\n", iter);
            if (nstack == 0)
                break;
            if (virDBusTypeStackPop(&stack, &nstack, &iter,
                                    &types, &nstruct, &narray) < 0)
                goto cleanup;
            printf("Popped iter=%p\n", iter);
            nstack--;

            if (!dbus_message_iter_close_container(iter, thisiter)) {
                if (thisiter != rootiter)
                    free(thisiter);
                printf("Cannot close container iterator\n");
                goto cleanup;
            }
            if (thisiter != rootiter)
                free(thisiter);
            continue;
        }

        t = types;
        if (narray != (size_t)-1) {
            narray--;
        } else {
            types++;
            nstruct--;
        }

        switch (*t) {
        case DBUS_TYPE_BYTE:
            SET_NEXT_VAL(unsigned char, int, unsigned char *, *t, "%d");
            break;

        case DBUS_TYPE_BOOLEAN:
            SET_NEXT_VAL(dbus_bool_t, int, int *, *t, "%d");
            break;

        case DBUS_TYPE_INT16:
            SET_NEXT_VAL(dbus_int16_t, int, short *, *t, "%d");
            break;

        case DBUS_TYPE_UINT16:
            SET_NEXT_VAL(dbus_uint16_t, unsigned int, unsigned short *,
                         *t, "%d");
            break;

        case DBUS_TYPE_INT32:
            SET_NEXT_VAL(dbus_int32_t, int, int *, *t, "%d");
            break;

        case DBUS_TYPE_UINT32:
            SET_NEXT_VAL(dbus_uint32_t, unsigned int, unsigned int *,
                         *t, "%u");
            break;

        case DBUS_TYPE_INT64:
            SET_NEXT_VAL(dbus_int64_t, long long, long long *, *t, "%lld");
            break;

        case DBUS_TYPE_UINT64:
            SET_NEXT_VAL(dbus_uint64_t, unsigned long long,
                         unsigned long long *, *t, "%llu");
            break;

        case DBUS_TYPE_DOUBLE:
            SET_NEXT_VAL(double, double, double *, *t, "%lf");
            break;

        case DBUS_TYPE_STRING:
        case DBUS_TYPE_OBJECT_PATH:
        case DBUS_TYPE_SIGNATURE:
            SET_NEXT_VAL(char *, char *, char **, *t, "%s");
            break;

        case DBUS_TYPE_ARRAY:
            arrayptr = NULL;
            if (t[1] == '&') {
                printf("Got array ref\n");
                t++;
                types++;
                nstruct--;
                arrayref = 1;
            } else {
                printf("Got array non-ref\n");
                arrayref = 0;
            }

            if (!(contsig = virDBusCopyContainerSignature(t, &skiplen, &siglen)))
                goto cleanup;

            if (arrayref && !virDBusIsAllowedRefType(contsig)) {
                printf("Got array ref but '%s' is not a single basic type or dict with matching key+value type\n",
                       contsig);
                goto cleanup;
            }

            if (narray == (size_t)-1) {
                types += skiplen;
                nstruct -= skiplen;
            }

            if (VIR_ALLOC(newiter) < 0)
                goto cleanup;
            printf("Contsig '%s' skip='%zu' len='%zu'\n", contsig, skiplen, siglen);
            if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
                                                  contsig, newiter))
                goto cleanup;

            nstack++;
            if (virDBusTypeStackPush(&stack, &nstack,
                                     iter, types,
                                     nstruct, narray) < 0) {
                free(newiter);
                goto cleanup;
            }
            free(contsig);
            iter = newiter;
            newiter = NULL;
            types = t + 1;
            nstruct = skiplen;
            narray = (size_t)va_arg(args, int);
            if (arrayref)
                arrayptr = va_arg(args, void *);
            break;

        case DBUS_TYPE_VARIANT:
            vsig = va_arg(args, const char *);
            if (!vsig) {
                printf("Missing variant type signature\n");
                goto cleanup;
            }
            if (VIR_ALLOC(newiter) < 0)
                goto cleanup;
            if (!dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT,
                                                  vsig, newiter))
                goto cleanup;
            nstack++;
            if (virDBusTypeStackPush(&stack, &nstack,
                                     iter, types,
                                     nstruct, narray) < 0) {
                free(newiter);
                goto cleanup;
            }
            iter = newiter;
            newiter = NULL;
            types = vsig;
            nstruct = strlen(types);
            narray = (size_t)-1;
            break;

        case DBUS_STRUCT_BEGIN_CHAR:
        case DBUS_DICT_ENTRY_BEGIN_CHAR:
            if (!(contsig = virDBusCopyContainerSignature(t, &skiplen, &siglen)))
                goto cleanup;

            if (VIR_ALLOC(newiter) < 0)
                goto cleanup;
            printf("Contsig '%s' skip='%zu' len='%zu'\n", contsig, skiplen, siglen);
            if (!dbus_message_iter_open_container(iter,
                                                  *t == DBUS_STRUCT_BEGIN_CHAR ?
                                                  DBUS_TYPE_STRUCT : DBUS_TYPE_DICT_ENTRY,
                                                  NULL, newiter))
                goto cleanup;
            if (narray == (size_t)-1) {
                types += skiplen - 1;
                nstruct -= skiplen - 1;
            }

            nstack++;
            if (virDBusTypeStackPush(&stack, &nstack,
                                     iter, types,
                                     nstruct, narray) < 0) {
                free(newiter);
                goto cleanup;
            }
            free(contsig);
            iter = newiter;
            newiter = NULL;
            types = t + 1;
            nstruct = skiplen - 2;
            narray = (size_t)-1;

            break;

        default:
            printf("Unknown type '%x' in signature '%s'\n",(int)*t, types);
            goto cleanup;
        }
    }

    ret = 0;

 cleanup:
    while (nstack > 0) {
        DBusMessageIter *thisiter = iter;
        printf("Popping iter=%p\n", iter);
        virDBusTypeStackPop(&stack, &nstack, &iter,
                            &types, &nstruct, &narray);
        printf("Popped iter=%p\n", iter);
        nstack--;

        if (thisiter != rootiter)
            free(thisiter);
    }

    free(contsig);
    free(newiter);
    return ret;
}
# undef SET_NEXT_VAL

int
virDBusMessageEncodeArgs(DBusMessage* msg,
                         const char *types,
                         va_list args)
{
    DBusMessageIter iter;
    int ret = -1;

    memset(&iter, 0, sizeof(iter));

    dbus_message_iter_init_append(msg, &iter);

    ret = virDBusMessageIterEncode(&iter, types, args);

    return ret;
}

int virDBusCreateMethodV(DBusMessage **call,
                         const char *destination,
                         const char *path,
                         const char *iface,
                         const char *member,
                         const char *types,
                         va_list args)
{
    int ret = -1;

    if (!(*call = dbus_message_new_method_call(destination,
                                               path,
                                               iface,
                                               member))) {
        printf("Out of Memory!\n");
        goto cleanup;
    }

    if (virDBusMessageEncodeArgs(*call, types, args) < 0) {
        virDBusMessageUnref(*call);
        *call = NULL;
        goto cleanup;
    }

    ret = 0;
 cleanup:
    return ret;
}

static int
virDBusCall(DBusConnection *conn,
            DBusMessage *call,
            DBusMessage **replyout)

{
    DBusMessage *reply = NULL;
    DBusError localerror;
    int ret = -1;
    const char *iface, *member, *path, *dest;

    dbus_error_init(&localerror);

    iface = dbus_message_get_interface(call);
    member = dbus_message_get_member(call);
    path = dbus_message_get_path(call);
    dest = dbus_message_get_destination(call);

    printf("'%s.%s' on '%s' at '%s'",
          iface, member, path, dest);

    if (!(reply = dbus_connection_send_with_reply_and_block(conn,
                                                            call,
                                                            VIR_DBUS_METHOD_CALL_TIMEOUT_MILLIS,
                                                            &localerror))) {
        printf("'%s.%s' on '%s' at '%s' error %s: %s",
              iface, member, path, dest,
              localerror.name,
              localerror.message);
        goto cleanup;
    }

    printf("'%s.%s' on '%s' at '%s'",
          iface, member, path, dest);

    ret = 0;

 cleanup:
    dbus_error_free(&localerror);
    if (reply) {
        if (ret == 0 && replyout)
            *replyout = reply;
        else
            virDBusMessageUnref(reply);
    }
    return ret;
}


int virDBusCallMethod(DBusConnection *conn,
                      DBusMessage **replyout,
                      const char *destination,
                      const char *path,
                      const char *iface,
                      const char *member,
                      const char *types, ...)
{
    DBusMessage *call = NULL;
    int ret = -1;
    va_list args;

    va_start(args, types);
    ret = virDBusCreateMethodV(&call, destination, path,
                               iface, member, types, args);
    va_end(args);
    if (ret < 0)
        goto cleanup;

    ret = virDBusCall(conn, call, replyout);

 cleanup:
    virDBusMessageUnref(call);
    return ret;
}

/**
 *
 * @param msg
 * @param conn
 */
void reply_method_call(DBusMessage *msg, DBusConnection *conn)
{
    DBusMessage *reply;
    DBusMessageIter reply_arg;
    DBusMessageIter msg_arg;
    dbus_uint32_t serial = 0;

    pid_t pid;
    char reply_str[128];
    void *__value;
    char *__value_str;
    int __value_int;

    int ret;

    pid = getpid();

    //创建返回消息reply
    reply = dbus_message_new_method_return(msg);
    if (!reply)
    {
        printf("Out of Memory!\n");
        return;
    }

    //在返回消息中填入参数。
    snprintf(reply_str, sizeof(reply_str), DBUS_RECEIVER_REPLY_STR, pid);
    __value_str = reply_str;
    __value = &__value_str;

    dbus_message_iter_init_append(reply, &reply_arg);
    if (!dbus_message_iter_append_basic(&reply_arg, DBUS_TYPE_STRING, __value))
    {
        printf("Out of Memory!\n");
        goto out;
    }

    //从msg中读取参数，根据传入参数增加返回参数
    if (!dbus_message_iter_init(msg, &msg_arg))
    {
        printf("Message has NO Argument\n");
        goto out;
    }

    do
    {
        int ret = dbus_message_iter_get_arg_type(&msg_arg);
        if (DBUS_TYPE_STRING == ret)
        {
            dbus_message_iter_get_basic(&msg_arg, &__value_str);
            printf("I am %d, get Method Argument STRING: %s\n", pid,
                    __value_str);

            __value = &__value_str;
            if (!dbus_message_iter_append_basic(&reply_arg,
                    DBUS_TYPE_STRING, __value))
            {
                printf("Out of Memory!\n");
                goto out;
            }
        }
        else if (DBUS_TYPE_INT32 == ret)
        {
            dbus_message_iter_get_basic(&msg_arg, &__value_int);
            printf("I am %d, get Method Argument INT32: %d\n", pid,
                    __value_int);

            __value_int++;
            __value = &__value_int;
            if (!dbus_message_iter_append_basic(&reply_arg,
                    DBUS_TYPE_INT32, __value))
            {
                printf("Out of Memory!\n");
                goto out;
            }
        }
        else
        {
            printf("Argument Type ERROR\n");
        }

    } while (dbus_message_iter_next(&msg_arg));

    //发送返回消息
    if (!dbus_connection_send(conn, reply, &serial))
    {
        printf("Out of Memory\n");
        goto out;
    }

    dbus_connection_flush(conn);
out:
    dbus_message_unref(reply);
}

/* 监听D-Bus消息，我们在上次的例子中进行修改 */
void dbus_receive(void)
{
    DBusMessage *msg;
    DBusMessageIter arg;
    DBusConnection *connection;
    DBusError err;

    pid_t pid;
    char name[64];
    char rule[128];

    const char *path;
    void *__value;
    char *__value_str;
    int __value_int;

    int ret;

    pid = getpid();

    dbus_error_init(&err);
    //创建于session D-Bus的连接
    connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!connection)
    {
        if (dbus_error_is_set(&err))
            printf("Connection Error %s\n", err.message);

        goto out;
    }

    //设置一个BUS name
    if (0 == access(DBUS_CLIENT_PID_FILE, F_OK))
        snprintf(name, sizeof(name), "%s%d", DBUS_RECEIVER_BUS_NAME, pid);
    else
        snprintf(name, sizeof(name), "%s", DBUS_RECEIVER_BUS_NAME);

    printf("i am a receiver, PID = %d, name = %s\n", pid, name);

    ret = dbus_bus_request_name(connection, name,
                            DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
        if (dbus_error_is_set(&err))
            printf("Name Error %s\n", err.message);

        goto out;
    }

    //要求监听某个signal：来自接口test.signal.Type的信号
    snprintf(rule, sizeof(rule), DBUS_RECEIVER_SIGNAL_RULE, DBUS_RECEIVER_INTERFACE);
    dbus_bus_add_match(connection, rule, &err);
    dbus_connection_flush(connection);
    if (dbus_error_is_set(&err))
    {
        printf("Match Error %s\n", err.message);
        goto out;
    }

    while (1)
    {
        dbus_connection_read_write(connection, 0);

        msg = dbus_connection_pop_message(connection);
        if (msg == NULL)
        {
            sleep(1);
            continue;
        }

        path = dbus_message_get_path(msg);
        if (strcmp(path, DBUS_RECEIVER_PATH))
        {
            printf("Wrong PATH: %s\n", path);
            goto next;
        }

        printf("Get a Message\n");
        if (dbus_message_is_signal(msg, DBUS_RECEIVER_INTERFACE, DBUS_RECEIVER_SIGNAL))
        {
            printf("Someone Send me a Signal\n");
            if (!dbus_message_iter_init(msg, &arg))
            {
                printf("Message Has no Argument\n");
                goto next;
            }

            ret = dbus_message_iter_get_arg_type(&arg);
            if (DBUS_TYPE_STRING == ret)
            {
                dbus_message_iter_get_basic(&arg, &__value_str);
                printf("I am %d, Got Signal with STRING: %s\n",
                        pid, __value_str);
            }
            else if (DBUS_TYPE_INT32 == ret)
            {
                dbus_message_iter_get_basic(&arg, &__value_int);
                printf("I am %d, Got Signal with INT32: %d\n",
                        pid, __value_int);
            }
            else
            {
                printf("Argument Type ERROR\n");
                goto next;
            }
        }
        else if (dbus_message_is_method_call(msg, DBUS_RECEIVER_INTERFACE, DBUS_RECEIVER_METHOD))
        {
            printf("Someone Call My Method\n");
            reply_method_call(msg, connection);
        }
        else
        {
            printf("NOT a Signal OR a Method\n");
        }
next:
        dbus_message_unref(msg);
    }

out:
    dbus_error_free(&err);
}

/*
 * call a method
 */
static void dbus_send(int mode, char *type, void *value)
{
    DBusConnection *connection;
    DBusError err;
    DBusMessage *msg;
    DBusMessageIter arg;
    DBusPendingCall *pending;
    dbus_uint32_t serial;

    int __type;
    void *__value;
    char *__value_str;
    int __value_int;
    pid_t pid;
    int ret;

    pid = getpid();

    //Step 1: connecting session bus
    /* initialise the erroes */
    dbus_error_init(&err);

    /* Connect to Bus*/
    connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!connection)
    {
        if (dbus_error_is_set(&err))
            printf("Connection Err : %s\n", err.message);

       goto out1;
    }

    //step 2: 设置BUS name，也即连接的名字。
    ret = dbus_bus_request_name(connection, DBUS_SENDER_BUS_NAME,
                            DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
        if (dbus_error_is_set(&err))
            printf("Name Err : %s\n", err.message);

        goto out1;
    }


    if (!strcasecmp(type, "STRING"))
    {
        __type = DBUS_TYPE_STRING;
        __value_str = value;
        __value = &__value_str;
    }
    else if (!strcasecmp(type, "INT32"))
    {
        __type = DBUS_TYPE_INT32;
        __value_int = atoi(value);
        __value = &__value_int;
    }
    else
    {
        printf("Wrong Argument Type\n");
        goto out1;
    }


    if (mode == MODE_METHOD)
    {
        printf("Call app[bus_name]=%s, object[path]=%s, interface=%s, method=%s\n",
                DBUS_RECEIVER_BUS_NAME, DBUS_RECEIVER_PATH,
                DBUS_RECEIVER_INTERFACE, DBUS_RECEIVER_METHOD);

        //针对目的地地址，创建一个method call消息。
        //Constructs a new message to invoke a method on a remote object.
        msg = dbus_message_new_method_call(
                DBUS_RECEIVER_BUS_NAME, DBUS_RECEIVER_PATH,
                DBUS_RECEIVER_INTERFACE, DBUS_RECEIVER_METHOD);
        if (msg == NULL)
        {
            printf("Message NULL");
            goto out1;
        }

        dbus_message_iter_init_append(msg, &arg);
        if (!dbus_message_iter_append_basic(&arg, __type, __value))
        {
            printf("Out of Memory!");
            goto out2;
        }

        //发送消息并获得reply的handle 。Queues a message to send, as with dbus_connection_send() , but also returns a DBusPendingCall used to receive a reply to the message.
        if (!dbus_connection_send_with_reply(connection, msg, &pending, -1))
        {
            printf("Out of Memory!");
            goto out2;
        }

        if (pending == NULL)
        {
            printf("Pending Call NULL: connection is disconnected ");
            goto out2;
        }

        dbus_connection_flush(connection);
        dbus_message_unref(msg);

        //waiting a reply，在发送的时候，已经获取了method reply的handle，类型为DBusPendingCall。
        // block until we receive a reply， Block until the pending call is completed.
        dbus_pending_call_block(pending);
        // get the reply message，Gets the reply, or returns NULL if none has been received yet.
        msg = dbus_pending_call_steal_reply(pending);
        if (msg == NULL)
        {
            printf("Reply Null\n");
            goto out1;
        }

        // free the pending message handle
        dbus_pending_call_unref(pending);

        // read the Arguments
        if (!dbus_message_iter_init(msg, &arg))
        {
            printf("Message has no Argument!\n");
            goto out2;
        }

        do
        {
            int ret = dbus_message_iter_get_arg_type(&arg);
            if (DBUS_TYPE_STRING == ret)
            {
                dbus_message_iter_get_basic(&arg, &__value_str);
                printf("I am %d, get Method return STRING: %s\n", pid,
                        __value_str);
            }
            else if (DBUS_TYPE_INT32 == ret)
            {
                dbus_message_iter_get_basic(&arg, &__value_int);
                printf("I am %d, get Method return INT32: %d\n", pid,
                        __value_int);
            }
            else
            {
                printf("Argument Type ERROR\n");
            }

        } while (dbus_message_iter_next(&arg));

        printf("NO More Argument\n");
    }
    else if (mode == MODE_SIGNAL)
    {
        printf("Signal to object[path]=%s, interface=%s, signal=%s\n",
                DBUS_RECEIVER_PATH, DBUS_RECEIVER_INTERFACE, DBUS_RECEIVER_SIGNAL);

        //步骤3:发送一个信号
        //根据图，我们给出这个信号的路径（即可以指向对象），接口，以及信号名，创建一个Message
        msg = dbus_message_new_signal(DBUS_RECEIVER_PATH,
                            DBUS_RECEIVER_INTERFACE, DBUS_RECEIVER_SIGNAL);
        if (!msg)
        {
            printf("Message NULL\n");
            goto out1;
        }

        dbus_message_iter_init_append(msg, &arg);
        if (!dbus_message_iter_append_basic(&arg, __type, __value))
        {
            printf("Out of Memory!");
            goto out2;
        }

        //将信号从连接中发送
        if (!dbus_connection_send(connection, msg, &serial))
        {
            printf("Out of Memory!\n");
            goto out2;
        }

        dbus_connection_flush(connection);
        printf("Signal Send\n");
    }

out2:
    dbus_message_unref(msg);
out1:
    dbus_error_free(&err);
}

int main(int argc, char *argv[])
{
    int pid = atoi(argv[1]);
    char uuid[] = "1111222233334444";
    DBusError err;
    DBusConnection *connection;

    /* initialise the erroes */
    dbus_error_init(&err);

    /* Connect to Bus*/
    connection = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (!connection)
    {
        if (dbus_error_is_set(&err))
            printf("Connection Err : %s\n", err.message);
    }

    if (virDBusCallMethod(connection,
                          NULL,
                          "org.freedesktop.machine1",
                          "/org/freedesktop/machine1",
                          "org.freedesktop.machine1.Manager",
                          "CreateMachine",
                          "sayssusa(sv)",
                          "vmtest",
                          16,
                          uuid[0], uuid[1], uuid[2], uuid[3],
                          uuid[4], uuid[5], uuid[6], uuid[7],
                          uuid[8], uuid[9], uuid[10], uuid[11],
                          uuid[12], uuid[13], uuid[14], uuid[15],
                          "libvirt-qemu",
                          "vm",
                          pid,
                          "",
                          3,
                          "Slice", "s", "machine.slice",
                          "After", "as", 1, "libvirtd.service",
                          "Before", "as", 1, "virt-guest-shutdown.target") < 0)
        printf("call api error\n");

    return 0;
}

