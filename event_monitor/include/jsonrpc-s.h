/*
 * JSON rpc server header
 *
 * Copyright (c) 2023 SmartX Inc
 *
 * Authors:
 *  Hyman Huang(黄勇) <yong.huang@smartx.com>
 */

#ifndef JSONRPCC_H_
#define JSONRPCC_H_

#include "cJSON.h"

/*
 *
 * http://www.jsonrpc.org/specification
 *
 * code	message	meaning
 * -32700	Parse error	Invalid JSON was received by the server.
 * An error occurred on the server while parsing the JSON text.
 * -32600	Invalid Request	The JSON sent is not a valid Request object.
 * -32601	Method not found	The method does not exist / is not available.
 * -32602	Invalid params	Invalid method parameter(s).
 * -32603	Internal error	Internal JSON-RPC error.
 * -32000 to -32099	Server error	Reserved for implementation-defined server-errors.
 */

#define JRPC_PARSE_ERROR -32700
#define JRPC_INVALID_REQUEST -32600
#define JRPC_METHOD_NOT_FOUND -32601
#define JRPC_INVALID_PARAMS -32603
#define JRPC_INTERNAL_ERROR -32693

typedef struct {
	void *data;
	int error_code;
	char * error_message;
} jrpc_context;

typedef cJSON* (*jrpc_function)(jrpc_context *context, cJSON *params, cJSON* id);

typedef struct jrpc_procedure jrpc_procedure;
typedef jrpc_procedure *jrpc_procedure_ptr;

struct jrpc_procedure {
	char * name;
	jrpc_function function;
	void *data;
};

typedef struct jrpc_server jrpc_server;
typedef jrpc_server *jrpc_server_ptr;

struct jrpc_server {
	int port_number;
    int fd;
    int watch;
	int procedure_count;
	struct jrpc_procedure *procedures;
	int debug_level;
};

typedef struct jrpc_connection jrpc_connection;
typedef jrpc_connection *jrpc_connection_ptr;

struct jrpc_connection {
	int fd;
    int watch;
    /* rpc server we belong to */
    jrpc_server_ptr server;
	int pos;
	unsigned int buffer_size;
	char * buffer;
	int debug_level;
};

int jrpc_server_init(jrpc_server_ptr server, int port_number);

void jrpc_server_destroy(struct jrpc_server *server);

int jrpc_register_procedure(struct jrpc_server *server,
		jrpc_function function_pointer, char *name, void *data);

int jrpc_deregister_procedure(struct jrpc_server *server, char *name);

#endif
