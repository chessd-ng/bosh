#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>

#include "socket_util.h"

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

