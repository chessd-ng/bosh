#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "socket_monitor.h"
#include "http.h"
//#include "http_header.h"
#include "list.h"
#include "iksemel.h"

#define MAX_BUFFER_SIZE (1024*128)

typedef struct HttpConnection {
    char buffer[MAX_BUFFER_SIZE+1];
    size_t buffer_size;
    struct HttpServer* server;
    int socket_fd;
    int rid;
	list_iterator it;
	HttpHeader* header;
} HttpConnection;

typedef struct HttpRequest {
	HttpConnection* connection;
	HttpHeader* header;
	const char* data;
	size_t data_size;
} HttpRequest;

typedef void(*hs_request_callback)(void*, const HttpRequest*);

typedef struct HttpServer {
    list* http_connections;
    SocketMonitor* monitor;
    int socket_fd;
	hs_request_callback callback;
	void* user_data;
} HttpServer;

HttpServer* hs_new(iks* config, SocketMonitor* monitor, hs_request_callback callback, void* user_data);

void hs_delete(HttpServer* server);

HttpRequest* hr_new(HttpConnection* connection, const char* data, int data_size);

void hs_delete(HttpServer* server);

void hs_answer_request(HttpConnection* connection, const char* msg, size_t size);

#endif
