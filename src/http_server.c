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

#include <errno.h>

#include "http_server.h"
#include "socket_util.h"
#include "log.h"

#include "allocator.h"

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

DECLARE_ALLOCATOR(HttpConnection, 1);
IMPLEMENT_ALLOCATOR(HttpConnection);

/*! \brief Send an http error response */
static void hc_report_error(HttpConnection* connection, const char* msg) {
	char* body = NULL;
	char* header = NULL;

	asprintf(&body, HTML_ERROR, msg);
	header = make_http_head(500, strlen(msg));

	send(connection->socket_fd, header, strlen(header), 0);
	send(connection->socket_fd, body, strlen(body), 0);

	free(body);
	free(header);
}

/*! \brief Release the connection's resources */
static void hc_delete(HttpConnection* connection) {
    HttpServer* server = connection->server;

    log(INFO, "closed");

    /* stop monitoring socket */
    sm_remove_socket(server->monitor, connection->socket_fd);

    /* close the socket */
    close(connection->socket_fd);

    /* erase the connection from the list of connections */
	list_erase(connection->it);

    /* free header */
    if(connection->header != NULL) {
        http_delete(connection->header);
        connection->header = NULL;
    }

    /* free memory */
    HttpConnection_free(connection);
}

static void hc_read_body(void* _connection);
static void hc_read_header(void* _connection);

/*! \brief Create an http connetion */
static HttpConnection* hc_create(HttpServer* server, int socket_fd) {
    HttpConnection* connection;
    
    /* alloc memory for the struct */
    connection = HttpConnection_alloc();

    /* init values */
    connection->buffer_size = 0;
    connection->server = server;
    connection->socket_fd = socket_fd;
    connection->header = NULL;

    /* insert the conenction into the connection list */
	connection->it = list_push_back(server->http_connections, connection);

    /* start to monitor the connection */
    sm_add_socket(server->monitor, socket_fd, hc_read_header, connection);

    return connection;
}

/*! \brief Process an incoming message */
static void hc_process(HttpConnection* connection) {
    const char* tmp;
    const char* data;
    int length;
	HttpRequest hr;
    HttpServer* server = connection->server;

    /* get the content lenght */
    tmp = http_get_field(connection->header, "Content-Length");

    /* absence of content lenght is not supported */
    if(tmp == NULL) {
        log(WARNING, "invalid header");
		hc_report_error(connection, "Invalid http header");
        sm_replace_socket(connection->server->monitor, connection->socket_fd, hc_read_header, connection);
        return;
    }
    length = atoi(tmp);
    
    log(INFO, "processing %d bytes", length);

    /* find the beginning of the content */
    data = strstr(connection->buffer, HTTP_LINE_SEP HTTP_LINE_SEP) + 4;

    /* check if everything is here */
    if(connection->buffer_size >= (connection->buffer + connection->buffer_size) - data) {

        /* set the connection the wait for a header */
        sm_replace_socket(connection->server->monitor, connection->socket_fd, hc_read_header, connection);

        log(INFO, "delivering message");

        /* inform the request */
		hr.connection = connection;
		hr.header = connection->header;
		hr.data = data;
		hr.data_size = length;
		server->callback(server->user_data, &hr);

        /* free the header, we don't need it anymore */
		http_delete(connection->header);
		connection->header = NULL;

        /* reset the buffer */
        connection->buffer_size = 0;
    }
}

/*! \brief Read the content of a request */
static void hc_read_body(void* _connection) {
    HttpConnection* connection = _connection;
    int remaing_buffer;
    int bytes;
	
    /* compute the ramaining buffer space */
	remaing_buffer = MAX_BUFFER_SIZE - connection->buffer_size;

    /* receive the data */
    bytes = recv(connection->socket_fd,
                 connection->buffer + connection->buffer_size,
                 remaing_buffer,
                 0);

    if(bytes > 0) {
        /* precess the data */
		connection->buffer_size += bytes;
        hc_process(connection);
    } else if(bytes <= 0) {
        /* close the connection */
        log(INFO, "Connection closed.");
        hc_delete(connection);
    }
}

/*! \brief Read the header of a request */
static void hc_read_header(void* _connection) {
    int bytes;
    HttpConnection* connection = _connection;

    /* compute the ramaining buffer space */
    int remaing_buffer = MAX_BUFFER_SIZE - connection->buffer_size;

    /* receive some data */
    bytes = recv(connection->socket_fd,
                 connection->buffer + connection->buffer_size,
                 remaing_buffer,
                 0);

    if(bytes > 0) {
        /* update the buffer */
        connection->buffer_size += bytes;
        connection->buffer[connection->buffer_size] = 0;

        /* parse the header */
        connection->header = http_parse(connection->buffer);

        /* if the header is complete, parser the content */
        if(connection->header != NULL) {
            sm_replace_socket(connection->server->monitor, connection->socket_fd, hc_read_body, connection);
            hc_process(connection);
        }
            
    } else if(bytes <= 0) {
        /* close the connection */
        log(INFO, "Connection closed.");
        hc_delete(connection);
    }
}

/*! \breif Receive an incoming connection */
static void hs_accept(void* _server) {
    int client_fd;
    HttpServer* server = _server;

    /* accept the conenction */
    client_fd = accept(server->socket_fd, NULL, NULL);

    /* check for error */
    if(client_fd == -1) {
        perror("handle_connection");
        return;
    }

    log(INFO, "new connection accepted %d", server->socket_fd);

    /* create the http connection */
    hc_create(server, client_fd);

}

/*! \brief Create a new HTTP server
 *
 * \param config the configuratin from config file
 * \param monitor the monitor to be used
 * \param callback the function to be called when a request arrives
 * \param user_data the parameter to the callback
 *
 * \return A instance of the server
 */
HttpServer* hs_new(iks* config, SocketMonitor* monitor, hs_request_callback callback, void* user_data) {
    HttpServer* server;
    int fd;
    const char* str;
    int port;

    /* read the port to listen */
    if((str = iks_find_attrib(config, "port")) != NULL) {
        port = atoi(str);
    } else {
        port = HTTP_PORT;
    }

    /* create the socket */
    fd = listen_socket(port);
    if(fd == -1) {
        return NULL;
    }

    /* alloc memomry for the server struct */
    server = malloc(sizeof(HttpServer));

    /* init values */
    server->monitor = monitor;
    server->socket_fd = fd;
    server->http_connections = list_new();
	server->callback = callback;
	server->user_data = user_data;

    /* monitor the server socket for conenctions */
    sm_add_socket(server->monitor, fd, hs_accept, server);

    return server;
}

/*! \breif Close an http server */
void hs_delete(HttpServer* server) {

    /* close ann connections */
    while(!list_empty(server->http_connections)) {
        hc_delete(list_front(server->http_connections));
    }

    /* close the server socket */
    close(server->socket_fd);

    /* free the list */
    list_delete(server->http_connections, NULL);

    /* free the serverstruct */
    free(server);
}

/*! \brief Answer a pending request
 *
 * \param connection the connection of the request
 * \param msg is the content of the message
 * \param size is the size of the content
 */
void hs_answer_request(HttpConnection* connection, const char* msg, size_t size) {
    char* header;

    /* create the header */
	header = make_http_head(200, size);

    log(INFO, "%s", msg);

    /* send the header and the content */
    if(send(connection->socket_fd, header, strlen(header), 0) == -1) {
        log(ERROR, "Could send data to socket %d: %s", connection->socket_fd, strerror(errno));
    }
    if(send(connection->socket_fd, msg, size, 0) == -1) {
        log(ERROR, "Could send data to socket %d: %s", connection->socket_fd, strerror(errno));
    }

    /* free the header */
    free(header);
}

