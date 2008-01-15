#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

int listen_socket(int port);

int connect_socket(const char* host, int port);

#endif
