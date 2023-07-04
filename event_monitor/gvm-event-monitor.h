#ifndef __GVM_EVENT_MONITOR_H__
# define __GVM_EVENT_MONITOR_H__

#include "jsonrpc-s.h"

#define DEFAULT_RPC_PORT 12190

typedef struct _gemServer gemServer;
typedef gemServer *gemServerPtr;

struct _gemServer {
    jrpc_server rpc_server;
};

#endif /* __GVM_EVENT_MONITOR_H__ */
