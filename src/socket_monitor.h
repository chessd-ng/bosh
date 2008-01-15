#ifndef SM_MONITOR_H
#define SM_MONITOR_H

#include <poll.h>

#include "time.h"
#include "hash.h"
#include "list.h"

#define MAX_CLIENTS (1024*16)

typedef void (*Callback)(void*);

typedef struct SocketInfo {
    Callback callback;
    void* user_data;
	int id;
	int socket_fd;
	list_iterator it;
} SocketInfo;

typedef struct SocketMonitor {
    struct pollfd poll_list[MAX_CLIENTS];
    int n_clients;
	hash* socket_hash;
    list* clients;
} SocketMonitor;


SocketMonitor* sm_new();

void sm_delete(SocketMonitor* monitor);

void sm_add_socket(SocketMonitor* monitor, int socket_fd, Callback callback, void* user_data);

void sm_replace_socket(SocketMonitor* monitor, int socket_fd, Callback callback, void* user_data);

void sm_remove_socket(SocketMonitor* monitor, int socket_fd);

void sm_poll(SocketMonitor* monitor, time_type max_time);

#endif
