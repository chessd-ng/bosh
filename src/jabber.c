#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "jabber.h"
#include "socket_util.h"

#define JABBER_HEADER "<stream:stream xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' to='%s' xml:lang='en'>"

iksparser* jabber_connect(const char* host_addr, int port, void *user_data, iksStreamHook callback) {
	int socket_fd;
	iksparser* parser;
	char* tmp;

	socket_fd = connect_socket(host_addr, port);

	if(socket_fd == -1) {
		return NULL;
	}
	
	parser = iks_stream_new("jabber:client", user_data, callback);

	iks_connect_fd(parser, socket_fd);
	
	asprintf(&tmp, JABBER_HEADER, host_addr);

    send(socket_fd, tmp, strlen(tmp), 0);

	free(tmp);

	return parser;

}
