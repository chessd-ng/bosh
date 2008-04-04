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


#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>

#include "socket_util.h"

/*! \brief Create a socket to listen a port */
int listen_socket(int port) {

    int socket_fd;
    struct sockaddr_in addr_in;

    if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("listen_socket");
        return -1;
    }

    addr_in.sin_family=AF_INET;
    addr_in.sin_port=htons(port);
    addr_in.sin_addr.s_addr=INADDR_ANY;

    if(bind(socket_fd, (struct sockaddr*) &addr_in, sizeof(addr_in)) < 0) {
        perror("listen_socket");
        return -1;
    }

    if(listen(socket_fd, 1024) == -1 ) {
        perror("listen_socket");
        return -1;
    }

    return socket_fd;

}

/*! \brief Create a socket connected to host */
int connect_socket(const char* host, int port) {
    struct hostent *hp;
    struct sockaddr_in sa;
    int socket_fd;
    
    if((hp = gethostbyname(host)) == NULL) {
        perror("conect_socket");
        return -1;
    }

    memcpy((char *)&sa.sin_addr, (char *)hp->h_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons(port);

    if((socket_fd = socket(hp->h_addrtype, SOCK_STREAM, 0)) == -1) {
        perror("conect_socket");
        return -1;
    }

    if(connect(socket_fd, (struct sockaddr *) &sa, sizeof sa) < 0) {
        perror("conect_socket");
        close(socket_fd);
        return -1;
    } 

    return socket_fd;
}

