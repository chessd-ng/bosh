#ifndef JABBER_H
#define JABBER_H

#include <iksemel.h>

iksparser* jabber_connect(const char* host_addr, int port, void* user_data, iksStreamHook callback);

#endif
