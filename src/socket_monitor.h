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


#ifndef SM_MONITOR_H
#define SM_MONITOR_H

#include <poll.h>

#include "time.h"
#include "hash.h"
#include "list.h"

typedef void (*Callback)(int events, void* user_data);

/*! \brief Init the socket monitor. */
void sm_init();

/*! \brief Quit the socket monitor. */
void sm_quit();

/*! \brief Add a socket to the monitor. */
void sm_add_socket(int socket_fd, Callback callback, void* user_data,
        int events);

/*! \brief Replace the callback of a socket. */
void sm_replace_callback(int socket_fd, Callback callback, void* user_data);

/*! \brief Remove a socket from the monitor. */
void sm_remove_socket(int socket_fd, int events);

/*! \brief Poll the sockets for any activity. */
void sm_poll(time_type max_time);

#endif
