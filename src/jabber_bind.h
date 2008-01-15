#ifndef JABBER_BIND_H
#define JABBER_BIND_H

#include <iksemel.h>

#include "socket_monitor.h"
#include "http_server.h"

struct JabberBind;

typedef struct JabberClient {
    iksparser* parser;
    int socket_fd;
    int sid, rid;
    HttpConnection* connection;
    list* output_queue;
    int alive;
    time_type timestamp;
    time_type wait;
	list_iterator it;
	struct JabberBind* bind;
} JabberClient;

typedef struct JabberBind {
	list* jabber_connections;
	hash* sid_hash;
	SocketMonitor* monitor;
	HttpServer* server;

    int jabber_port;
    int session_timeout;
    int default_request_timeout;
} JabberBind;

JabberBind* jb_new(iks* config);

void jb_delete(JabberBind* jc);

void jb_run(JabberBind* jb);

#endif
