/*
 *   Copyright (c) 2007-2008 C3SL.
 *
 *   This file is part of Bosh.
 *
 *   Bosh is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   any later version.
 *
 *   Bosh is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <unistd.h>

#include "http_server.h"
#include "socket_util.h"
#include "log.h"

#define HTTP_PORT 8080

#define HTML_ERROR "<html><head> \
						<title>400 Bad Request</title> \
						</head><body> \
						<h1>Bad Request</h1> \
						<p>%s</p> \
						<hr> \
						</body></html>"

#define MAX_BUFFER_SIZE (1024*128)

struct HttpConnection {
    char buffer[MAX_BUFFER_SIZE+1];
    size_t buffer_size;
    struct HttpServer* server;
    int socket_fd;
    int rid;
	list_iterator it;
	HttpHeader* header;
};

struct HttpServer {
    list* http_connections;
    SocketMonitor* monitor;
    int socket_fd;
	hs_request_callback callback;
	void* user_data;
};

static void hs_report_error(HttpConnection* connection, const char* msg) {
	char* body = NULL;
	char* header = NULL;

	asprintf(&body, HTML_ERROR, msg);
	header = make_http_head(500, strlen(msg));

	send(connection->socket_fd, header, strlen(header), 0);
	send(connection->socket_fd, body, strlen(body), 0);

	free(body);
	free(header);
}

static void hs_release_connection(HttpServer* server, HttpConnection* connection) {
    log("closed");

    sm_remove_socket(server->monitor, connection->socket_fd);

    close(connection->socket_fd);

	list_erase(connection->it);

    if(connection->header != NULL) {
        http_delete(connection->header);
        connection->header = NULL;
    }

    free(connection);
}

static HttpConnection* hs_get_connection(HttpServer* server, int socket_fd) {
    HttpConnection* connection = malloc(sizeof(HttpConnection));

    connection->buffer_size = 0;
    connection->server = server;
    connection->socket_fd = socket_fd;
    connection->header = NULL;
	connection->it = list_push_back(server->http_connections, connection);

    return connection;
}

static void hs_read_body(void* _connection);
static void hs_read_header(void* _connection);

static void hs_process(HttpServer* server, HttpConnection* connection) {
    const char* tmp;
    const char* data;
    int length;
	HttpRequest hr;

    tmp = http_get_field(connection->header, "Content-Length");

    if(tmp == NULL) {
        log("invalid header");
		hs_report_error(connection, "Invalid http header");
        sm_replace_socket(connection->server->monitor, connection->socket_fd, hs_read_header, connection);
        return;
    }

    length = atoi(tmp);
    
    log("processing %d bytes", length);

    data = strstr(connection->buffer, HTTP_LINE_SEP HTTP_LINE_SEP) + 4;

    if(connection->buffer_size >= (connection->buffer + connection->buffer_size) - data) {

        sm_replace_socket(connection->server->monitor, connection->socket_fd, hs_read_header, connection);

        log("delivering message");

		hr.connection = connection;
		hr.header = connection->header;
		hr.data = data;
		hr.data_size = length;
		
		server->callback(server->user_data, &hr);

		http_delete(hr.header);
		connection->header = NULL;

        connection->buffer_size = 0;
    }
}

static void hs_read_body(void* _connection) {
    HttpConnection* connection = _connection;
    int remaing_buffer;
    int bytes;
	
	remaing_buffer = MAX_BUFFER_SIZE - connection->buffer_size;

    bytes = recv(connection->socket_fd,
            connection->buffer + connection->buffer_size,
            remaing_buffer,
            0);

    if(bytes > 0) {
		connection->buffer_size += bytes;
        hs_process(connection->server, connection);
    } else if(bytes == 0) {
        log("Connection closed.");
        hs_release_connection(connection->server, connection);
    }
}

static void hs_read_header(void* _connection) {
    int bytes;
    HttpConnection* connection = _connection;

    int remaing_buffer = MAX_BUFFER_SIZE - connection->buffer_size;

    bytes = recv(connection->socket_fd,
            connection->buffer + connection->buffer_size,
            remaing_buffer,
            0);

    if(bytes > 0) {
        connection->buffer_size += bytes;
        connection->buffer[connection->buffer_size] = 0;

        connection->header = http_parse(connection->buffer);

        if(connection->header != NULL) {
            sm_replace_socket(connection->server->monitor, connection->socket_fd, hs_read_body, connection);

            hs_process(connection->server, connection);
        }
            
    } else if(bytes == 0) {
        log("Connection closed.");
        hs_release_connection(connection->server, connection);
    }
}

static void hs_accept(void* _server) {
    int client_fd;
    HttpServer* server = _server;

    client_fd = accept(server->socket_fd, NULL, NULL);
    if(client_fd == -1) {
        perror("handle_connection");
        return;
    }
    log("new connection accepted %d", server->socket_fd);

    HttpConnection* connection = hs_get_connection(server, client_fd);

    sm_add_socket(server->monitor, client_fd, hs_read_header, connection);
}

HttpServer* hs_new(iks* config, SocketMonitor* monitor, hs_request_callback callback, void* user_data) {
    HttpServer* server;
    int fd;
    const char* str;
    int port;

    if((str = iks_find_attrib(config, "port")) != NULL) {
        port = atoi(str);
    } else {
        port = HTTP_PORT;
    }

    fd = listen_socket(port);
    if(fd == -1) {
        return NULL;
    }

    server = malloc(sizeof(HttpServer));
    server->monitor = monitor;
    server->socket_fd = fd;
    server->http_connections = list_new();
	server->callback = callback;
	server->user_data = user_data;

    sm_add_socket(server->monitor, fd, hs_accept, server);

	srand48(get_time());

    return server;
}

void hs_delete(HttpServer* server) {

    while(!list_empty(server->http_connections)) {
        hs_release_connection(server, list_front(server->http_connections));
    }

    close(server->socket_fd);

    list_delete(server->http_connections, NULL);

    free(server);
}

void hs_answer_request(HttpConnection* connection, const char* msg, size_t size) {
    char* header;

	header = make_http_head(200, size);

    log("%s", msg);

    send(connection->socket_fd, header, strlen(header), 0);
    send(connection->socket_fd, msg, size, 0);

    free(header);
}

