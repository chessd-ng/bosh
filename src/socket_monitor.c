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

static unsigned int hash_int(void* i) {
	return *(int*)i;
}

static int cmp_int(void* i1, void* i2) {
	return *(int*)i1 == *(int*)i2;
}

SocketMonitor* sm_new() {
    SocketMonitor* monitor = malloc(sizeof(SocketMonitor));
    monitor->n_clients = 0;
	monitor->socket_hash = hash_new(hash_int, cmp_int);
	monitor->clients = list_new();
    return monitor;
}

void sm_delete(SocketMonitor* monitor) {
	hash_delete(monitor->socket_hash);
	list_delete(monitor->clients, free);
    free(monitor);
}

void sm_add_socket(SocketMonitor* monitor, int socket_fd, Callback callback, void* user_data) {
    int id;
	SocketInfo* si;
	struct pollfd* pf;

    id = monitor->n_clients;

	si = malloc(sizeof(SocketInfo));
	pf = &monitor->poll_list[id];

    si->callback = callback;
    si->user_data = user_data;
	si->id = id;
	si->socket_fd = socket_fd;
	si->it = list_push_back(monitor->clients, si);

    pf->fd = socket_fd;
    pf->events = POLLIN;
    pf->revents = 0;

    monitor->n_clients++;

	hash_insert(monitor->socket_hash, &si->socket_fd, si);
}

void sm_replace_socket(SocketMonitor* monitor, int socket_fd, Callback callback, void* user_data) {
	SocketInfo* si;

	si = hash_find(monitor->socket_hash, &socket_fd);

	si->callback = callback;
	si->user_data = user_data;
}

void sm_remove_socket(SocketMonitor* monitor, int socket_fd) {
	SocketInfo* si;
    int id, old_id;

	si = hash_erase(monitor->socket_hash, &socket_fd);
	list_erase(si->it);
	id = si->id;

	old_id = monitor->n_clients - 1;

	monitor->poll_list[id] = monitor->poll_list[old_id];
	monitor->n_clients--;

	free(si);

	if(old_id != id) {
		si = hash_find(monitor->socket_hash, &monitor->poll_list[id]);
		si->id = id;
	}
}

void sm_poll(SocketMonitor* monitor, time_type max_time) {
    int ret, i;
	SocketInfo* si;

	log("sockets = %d", monitor->n_clients);
	ret = poll(monitor->poll_list, monitor->n_clients, max_time);
	if(ret > 0) {
		for(i = 0; i < monitor->n_clients; ++i) {
            /* if the socket was removed during the execution of the callback? */
			if((monitor->poll_list[i].revents & POLLIN) != 0) {
				si = hash_find(monitor->socket_hash, &monitor->poll_list[i].fd);
				si->callback(si->user_data);
			}
		}
	} else if(ret < 0) {
		log("%s", strerror(errno));
	}

}

