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


#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "socket_monitor.h"
#include "allocator.h"
#include "log.h"

#define MAX_SOCKETS (1024*16)

static unsigned int hash_int(const int i) {
    return i;
}

static int cmp_int(int i1, int i2) {
    return i1 == i2;
}

DECLARE_HASH(int, hash_int, cmp_int);
IMPLEMENT_HASH(int);

typedef struct SocketMonitor {
    struct pollfd poll_list[MAX_SOCKETS];
    int_hash* socket_hash;
    list* sockets;
    int socket_count;
} SocketMonitor;

typedef struct SocketInfo {
    Callback callback;
    void* user_data;
    int id;
    int socket_fd;

    /* iterator to this socket in the monitor list */
    list_iterator it;
} SocketInfo;

DECLARE_ALLOCATOR(SocketInfo, 1024);
IMPLEMENT_ALLOCATOR(SocketInfo);

static SocketMonitor* monitor = NULL;

SocketMonitor* sm_new() {
    SocketMonitor* monitor = malloc(sizeof(SocketMonitor));
    monitor->socket_count = 0;
    monitor->socket_hash = int_hash_new();
    monitor->sockets = list_new();
    return monitor;
}

void sm_delete(SocketMonitor* monitor) {
    list_iterator it;

    list_foreach(it, monitor->sockets) {
        SocketInfo_free(list_iterator_value(it));
    }
    int_hash_delete(monitor->socket_hash);
    list_delete(monitor->sockets, NULL);
    free(monitor);
}

void sm_add_socket(int socket_fd, Callback callback, void* user_data,
        int events) {

    int id;
    SocketInfo* si;
    struct pollfd* pf;

    /* check if the socket is already there */
    si = int_hash_find(monitor->socket_hash, socket_fd);
    if(si != NULL) {
        si->callback = callback;
        si->user_data = user_data;
        monitor->poll_list[si->id].events |= events;
        return;
    }

    /* get an id */
    id = monitor->socket_count;
    monitor->socket_count++;

    /* create and init si struct */
    si = SocketInfo_alloc();
    si->callback = callback;
    si->user_data = user_data;
    si->id = id;
    si->socket_fd = socket_fd;
    si->it = list_push_back(monitor->sockets, si);

    /* init pollfd entry */
    pf = &monitor->poll_list[id];
    pf->fd = socket_fd;
    pf->events = events;
    pf->revents = 0;

    /* insert socket to the socket hash */
    int_hash_insert(monitor->socket_hash, si->socket_fd, si);
}

void sm_replace_callback(int socket_fd, Callback callback, void* user_data) {
    SocketInfo* si;

    /* find socket */
    si = int_hash_find(monitor->socket_hash, socket_fd);

    /* replace callback */
    si->callback = callback;
    si->user_data = user_data;
}

void sm_remove_socket(int socket_fd, int events) {
    SocketInfo* si;
    int id, old_id;

    /* find socket info */
    si = int_hash_find(monitor->socket_hash, socket_fd);
    if(si == NULL) {
        return;
    }

    /* remove unwantd events */
    id = si->id;
    monitor->poll_list[id].events &= ~events;

    /* if there is any event left don't remove it */
    if(monitor->poll_list[id].events != 0) {
        return;
    }

    /* erase the socket from the hash and the list */
    si = int_hash_erase(monitor->socket_hash, socket_fd);
    list_erase(si->it);
    SocketInfo_free(si);
    monitor->socket_count--;

    /* erase the pollfd entry by swaping with the last one */
    old_id = monitor->socket_count;
    if(old_id != id) {
        monitor->poll_list[id] = monitor->poll_list[old_id];
        si = int_hash_find(monitor->socket_hash, monitor->poll_list[id].fd);
        si->id = id;
    }
}

void sm_poll(time_type max_time) {
    int ret, i;
    SocketInfo* si;

    log(INFO, "sockets = %d", monitor->socket_count);

    /* poll for events and call the callbacks */
    ret = poll(monitor->poll_list, monitor->socket_count, max_time);
    if(ret > 0) {
        for(i = 0; i < monitor->socket_count; ++i) {
            /* FIXME if the socket was removed during the execution of the
             * callback? */
            /* this is not really bad, because the socket that replaces the
             * erased will be processed later */
            if((monitor->poll_list[i].revents) != 0) {
                log(INFO, "Event on socket %d", monitor->poll_list[i].fd);
                si = int_hash_find(monitor->socket_hash,
                        monitor->poll_list[i].fd);
                si->callback(monitor->poll_list[i].revents, si->user_data);
            }
        }
    } else if(ret < 0) {
        log(ERROR, "%s", strerror(errno));
    }
}


void sm_init() {
    monitor = sm_new();
}

void sm_quit() {
    sm_delete(monitor);
}
