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
#include "log.h"

typedef struct SocketInfo {
    Callback callback;
    void* user_data;
	int id;
	int socket_fd;

    /* iterator to this socket in the monitor list */
	list_iterator it;
} SocketInfo;

struct SocketMonitor {
    struct pollfd poll_list[MAX_CLIENTS];
    int n_clients;
	hash* socket_hash;
    list* clients;
};

static unsigned int hash_int(const void* i) {
	return *(const int*)i;
}

static int cmp_int(const void* i1, const void* i2) {
	return *(const int*)i1 == *(const int*)i2;
}

IMPLEMENT_HASH(socket, hash_int, cmp_int);

SocketMonitor* sm_new() {
    SocketMonitor* monitor = malloc(sizeof(SocketMonitor));
    monitor->n_clients = 0;
	monitor->socket_hash = socket_hash_new();
	monitor->clients = list_new();
    return monitor;
}

void sm_delete(SocketMonitor* monitor) {
	socket_hash_delete(monitor->socket_hash);
	list_delete(monitor->clients, free);
    free(monitor);
}

void sm_add_socket(SocketMonitor* monitor, int socket_fd, Callback callback, void* user_data) {
    int id;
	SocketInfo* si;
	struct pollfd* pf;

    /* get a id */
    id = monitor->n_clients;
    monitor->n_clients++;

    /* ceate and init si struct */
	si = malloc(sizeof(SocketInfo));
    si->callback = callback;
    si->user_data = user_data;
	si->id = id;
	si->socket_fd = socket_fd;
	si->it = list_push_back(monitor->clients, si);

    /* init pollfd entry */
	pf = &monitor->poll_list[id];
    pf->fd = socket_fd;
    pf->events = POLLIN;
    pf->revents = 0;


    /* insert socket to the socket hash */
	socket_hash_insert(monitor->socket_hash, &si->socket_fd, si);
}

void sm_replace_socket(SocketMonitor* monitor, int socket_fd, Callback callback, void* user_data) {
	SocketInfo* si;

    /* find socket */
	si = socket_hash_find(monitor->socket_hash, &socket_fd);
    
	/* replace callback */
    si->callback = callback;
	si->user_data = user_data;
}

void sm_remove_socket(SocketMonitor* monitor, int socket_fd) {
	SocketInfo* si;
    int id, old_id;

    /* erase the socket from the hash and the list */
	si = socket_hash_erase(monitor->socket_hash, &socket_fd);
	id = si->id;
	list_erase(si->it);
	free(si);
	monitor->n_clients --;

    /* erase the pollfd entry by swaping with the last one */
	old_id = monitor->n_clients;
	if(old_id != id) {
        monitor->poll_list[id] = monitor->poll_list[old_id];
		si = socket_hash_find(monitor->socket_hash, &monitor->poll_list[id]);
		si->id = id;
	}
}

void sm_poll(SocketMonitor* monitor, time_type max_time) {
    int ret, i;
	SocketInfo* si;

	log(INFO, "sockets = %d", monitor->n_clients);

    /* poll for events and call the callbacks */
	ret = poll(monitor->poll_list, monitor->n_clients, max_time);
	if(ret > 0) {
		for(i = 0; i < monitor->n_clients; ++i) {
            /* FIXME if the socket was removed during the execution of the callback? */
            /* this is not really bad, because the socket that replaces the erased
             * will be processes later */
			if((monitor->poll_list[i].revents & POLLIN) != 0) {
				si = socket_hash_find(monitor->socket_hash, &monitor->poll_list[i].fd);
				si->callback(si->user_data);
			}
		}
	} else if(ret < 0) {
        /* we got a error */
		log(ERROR, "%s", strerror(errno));
	}
}

