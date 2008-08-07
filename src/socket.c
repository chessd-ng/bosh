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

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>

#include <errno.h>

#include "socket.h"
#include "list.h"
#include "allocator.h"

#include "socket_monitor.h"

#include "log.h"

typedef struct QueueItem {
    void* buffer;
    size_t len, offset;
} QueueItem;

struct Socket {
    int fd;

    DataCallback data_callback;
    void* data_data;

    AcceptCallback accept_callback;
    void* accept_data;

    ConnectCallback connect_callback;
    void* connect_data;

    list* output_queue;

    SocketStatus status;
};

DECLARE_ALLOCATOR(QueueItem, 512);
IMPLEMENT_ALLOCATOR(QueueItem);

DECLARE_ALLOCATOR(Socket, 128);
IMPLEMENT_ALLOCATOR(Socket);

/*! \brief Create a new socket */
Socket* sock_new() {
    Socket* sock;

    /* alloc memory */
    sock = Socket_alloc();

    /* set initial values */
    sock->fd = -1;
    sock->data_callback = NULL;
    sock->data_data = NULL;
    sock->accept_callback = NULL;
    sock->accept_data = NULL;
    sock->connect_callback = NULL;
    sock->connect_data = NULL;
    sock->status = SOCKET_IDLE;

    /* create the output queue */
    sock->output_queue = list_new();

    return sock;
}

/*! \breif Free a queue item */
void item_delete(QueueItem* item) {
    free(item->buffer);
    QueueItem_free(item);
}

/*! \brief Close the socket */
void sock_close(Socket* sock) {
    /* close the socket */
    if(sock->fd != -1) {
        sm_remove_socket(sock->fd, POLLIN | POLLOUT);
        close(sock->fd);
    }

    /* free remaining items in queue */
    while(!list_empty(sock->output_queue)) {
        item_delete(list_pop_front(sock->output_queue));
    }
    list_delete(sock->output_queue, NULL);

    sock->status = SOCKET_IDLE;
}

/*! \brief Delete a socket */
void sock_delete(Socket* sock) {
    sock_close(sock);
    Socket_free(sock);
}

/*" \brief Send data that is in the queue to the socket */
void sock_flush_data(Socket* sock) {
    QueueItem* item;
    ssize_t ret;

    while(!list_empty(sock->output_queue)) {
        item = list_front(sock->output_queue);

        while(item->offset < item->len) {
            ret = send(sock->fd, item->buffer + item->offset,
                    item->len - item->offset, MSG_NOSIGNAL | MSG_DONTWAIT
                    |MSG_MORE);

            if(ret > 0) {
                item->offset += ret;
            } else if(ret == 0) {
                break;
            } else if(ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            } else {
                log(WARNING, "Failed to write to socket %d: %s", sock->fd,
                        strerror(errno));
                sock_close(sock);
                break;
            }
        }

        if(item->offset < item->len) {
            break;
        } else {
            item_delete(list_pop_front(sock->output_queue));
        }
    }

    if(list_empty(sock->output_queue)) {
        sm_remove_socket(sock->fd, POLLOUT);
    }
}

/*" \brief Handle vents in the socket */
void socket_callback(int events, void* user_data) {
    int opt, success;
    socklen_t opt_len;
    Socket* sock = user_data;

    /* Check if we can write */
    if(events & POLLOUT) {
        if(sock->status == SOCKET_CONNECTING) {
            opt_len = sizeof(opt);
            getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &opt, &opt_len);
            if(opt != 0) {
                sock->status = SOCKET_IDLE;
                success = 0;
            } else {
                sock->status = SOCKET_CONNECTED;
                success = 1;
                if(sock->data_callback != NULL) {
                    sm_add_socket(sock->fd, socket_callback, sock, POLLIN);
                }
            }
            if(sock->connect_callback != NULL) {
                sock->connect_callback(success, sock->connect_data);
            }
        }
        sock_flush_data(sock);
    }

    /* check if we can read */
    if(events & POLLIN) {
        if(sock->status == SOCKET_LISTENING) {
            if(sock->accept_callback != NULL) {
                sock->accept_callback(sock->accept_data);
            } else {
                sm_remove_socket(sock->fd, POLLIN);
            }
        } else if(sock->status == SOCKET_CONNECTED) {
            if(sock->data_callback != NULL) {
                sock->data_callback(sock->data_data);
            } else {
                sm_remove_socket(sock->fd, POLLIN);
            }
        } else {
            log(ERROR, "POLLIN event on idle socket");
        }
    }

}

/*! \brief Asynchronously connect to the given host.
 *
 * Returns 1 on success 0 otherwise */
int sock_connect(Socket* sock, const char* host, int port) {
    
    struct hostent *hp;
    struct sockaddr_in sa;
    int arg, ret;

    /* resolve host address */
    if((hp = gethostbyname(host)) == NULL) {
        log(ERROR, "Unable to resolve host %s: %s", host, strerror(errno));
        return 0;
    }

    /* create the socket */
    sock->fd = socket(hp->h_addrtype, SOCK_STREAM, 0);
    if(sock->fd == -1) {
        log(ERROR, "Unable to create socket: %s", strerror(errno));
        return 0;
    }

    /* set socket as non blocking */
    arg = fcntl(sock->fd, F_GETFL, NULL); 
    arg |= O_NONBLOCK; 
    fcntl(sock->fd, F_SETFL, arg); 

    /* set up host address */
    memset(&sa, 0, sizeof(sa));
    memcpy((char *)&sa.sin_addr, (char *)hp->h_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons(port);

    /* connect to the host */
    ret = connect(sock->fd, (struct sockaddr *) &sa, sizeof sa);
    if(ret < 0 && errno != EINPROGRESS) {
        log(ERROR, "Unable to connect to %s:%d: %s", host, port,
                strerror(errno));
        close(sock->fd);
        return 0;
    } 

    /* wait the connection to be completed */
    sm_add_socket(sock->fd, socket_callback, sock, POLLOUT);

    sock->status = SOCKET_CONNECTING;

    return 1;
}

/*! \brief Receive data from the socket
 *
 * Returns the number of bytes read. When the connection by the remote host
 * and this function is called, the socket status will change to idle. This
 * function never blocks, if there is no data to read it will return 0 */
ssize_t sock_recv(Socket* sock, void* buffer, size_t len) {
    ssize_t ret;

    /* read data */
    ret = recv(sock->fd, buffer, len, MSG_DONTWAIT | MSG_NOSIGNAL);

    /* parse the result */
    if(ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        ret = 0;
    } else if(ret == -1) {
        log(WARNING, "Unable to read socket %d: %s", sock->fd, strerror(errno));
        sock_close(sock);
        ret = 0;
    } else if(ret == 0) {
        sock_close(sock);
    }

    return ret;
}

/*! \brief Send data to the socket
 *
 * This function never blocks, if the send would block, the buffer is queued
 * to be sent later. The ownership of the buffer is passed to the calee
 * function. Don't expect the buffer pointer to be valid after this call. */
void sock_send(Socket* sock, void* buffer, size_t len, int more) {
    int was_empty;
    QueueItem* item;

    was_empty = list_empty(sock->output_queue);

    if(buffer != NULL) {
        item = QueueItem_alloc();

        item->buffer = buffer;
        item->len = len;
        item->offset = 0;
        list_push_back(sock->output_queue, item);
    }

    if(sock->status == SOCKET_CONNECTED && was_empty && (!more)) {
        sock_flush_data(sock);
    }

    if(was_empty && !list_empty(sock->output_queue)) {
        sm_add_socket(sock->fd, socket_callback, sock, POLLOUT);
    }
}

/*! \brief Start listening on the given port
 *
 * This function will not block, instead, the accept callback will be called
 * if there is an incoming connection. */
int sock_listen(Socket* sock, int port) {
    struct sockaddr_in addr_in;
    int opt, arg;

    /* create the socket */
    if((sock->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        log(ERROR, "Unable to create a socket: %s", port, strerror(errno));
        return 0;
    }

    /* set socket as non blocking */
    arg = fcntl(sock->fd, F_GETFL, NULL); 
    arg |= O_NONBLOCK; 
    fcntl(sock->fd, F_SETFL, arg); 

    /* set REUSEADDR so we can restart bosh quickly */
    opt = 1;
    if(setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt,
                sizeof(opt)) == -1) {
        log(WARNING, "Unable to set REUSEADDR option: %s", strerror(errno));
    }

    /* bind to the port */
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    addr_in.sin_addr.s_addr = INADDR_ANY;
    if(bind(sock->fd, (struct sockaddr*) &addr_in, sizeof(addr_in)) < 0) {
        log(ERROR, "Unable to bind to port %d: %s", port, strerror(errno));
        close(sock->fd);
        return 0;
    }

    /* start listening */
    if(listen(sock->fd, 1024) == -1 ) {
        log(ERROR, "Unable to listen port %d: %s", port, strerror(errno));
        close(sock->fd);
        return 0;
    }

    /* set status */
    sock->status = SOCKET_LISTENING;

    /* monitor incoming connections */
    if(sock->accept_callback != NULL) {
        sm_add_socket(sock->fd, socket_callback, sock, POLLIN);
    }

    return 1;
}

/*! \brief Accepts an incoming connection.
 *
 * This function should be called when the accept callback is called */
Socket* sock_accept(Socket* sock) {
    Socket* client;
    int fd;

    fd = accept(sock->fd, NULL, NULL);

    if(fd == -1) {
        if(errno != EAGAIN && errno != EWOULDBLOCK) {
            log(WARNING, "Error when trying to accept connection: %s",
                    strerror(errno));
            sock_close(sock);
        }
        return NULL;
    }

    client = sock_new();

    client->fd = fd;
    client->status = SOCKET_CONNECTED;

    return client;
}

/*! \brief Returns the socket current status */
SocketStatus sock_status(Socket* sock) {
    return sock->status;
}

/*! \brief Set the data callback.
 *
 * This callback will be called whenever there is data to read from the
 * socket. */
void sock_set_data_callback(Socket* sock, DataCallback callback,
        void* user_data) {

    sock->data_callback = callback;
    sock->data_data = user_data;

    if(sock->status == SOCKET_CONNECTED && sock->data_callback != NULL) {
        sm_add_socket(sock->fd, socket_callback, sock, POLLIN);
    }
}

/*! \brief Set the accept callback.
 *
 * This callback will be called whenever there is * an incoming connection to
 * the socket */
void sock_set_accept_callback(Socket* sock, AcceptCallback callback,
        void* user_data) {

    sock->accept_callback = callback;
    sock->accept_data = user_data;

    if(sock->status == SOCKET_LISTENING && sock->accept_callback != NULL) {
        sm_add_socket(sock->fd, socket_callback, sock, POLLIN);
    }
}

/*! \brief Set the conenct callback.
 *
 * This callback will be called when a connection request is completed. The
 * parameter success of the callback will be 1 if the connection was
 * succesfull, 0 otherwise. */
void sock_set_connect_callback(Socket* sock, ConnectCallback callback,
        void* user_data) {

    sock->connect_callback = callback;
    sock->connect_data = user_data;
}
