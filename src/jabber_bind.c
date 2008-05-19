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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>

#include <signal.h>

#include <inttypes.h>

#include "http_server.h"
#include "jabber_bind.h"
#include "jabber.h"
#include "log.h"
#include "allocator.h"

#define JABBER_PORT 5222

#define SESSION_TIMEOUT (30000)

#define DEFAULT_REQUEST_TIMEOUT (30000)

#define MESSAGE_WRAPPER "<body xmlns='http://jabber.org/protocol/httpbind'>%s</body>"

#define EMPTY_RESPONSE "<body xmlns='http://jabber.org/protocol/httpbind'/>"

#define SESSION_RESPONSE "<body sid='%" PRId64 "' ver='1.6' xmlns='http://jabber.org/protocol/httpbind'/>"

#define TERMINATE_SESSIN_RESPONSE "<body type='terminate' xmlns='http://jabber.org/protocol/httpbind'/>"

#define ERROR_RESPONSE "<body type='%s' condition='%s' xmlns='http://jabber.org/protocol/httpbind'/>"

typedef uint64_t uint64;

static int compare_sid(uint64_t s1, uint64_t s2) {
    return s1 == s2;
}

static unsigned int hash_sid(uint64_t s) {
    return s & 0xffffffff;
}

DECLARE_HASH(uint64, hash_sid, compare_sid);
IMPLEMENT_HASH(uint64);

enum BIND_ERROR_CODE {
    SID_NOT_FOUND = 0,
    BAD_FORMAT = 1,
    CONNECTION_FAILED = 2
};

const char ERROR_TABLE[][2][64] = {
    {"terminate", "item-not-found"},
    {"terminate", "bad-request"},
    {"terminate", "host-gone"}
};

volatile int running;

typedef struct JabberClient {
    iksparser* parser;
    int socket_fd;
    uint64_t sid, rid;
    HttpConnection* connection;
    list* output_queue;
    int alive;
    time_type timestamp;
    time_type wait;
	list_iterator it;
	struct JabberBind* bind;
} JabberClient;


struct JabberBind {
	list* jabber_connections;
	uint64_hash* sids;
	SocketMonitor* monitor;
	HttpServer* server;

    int jabber_port;
    int session_timeout;
    int default_request_timeout;
};

DECLARE_ALLOCATOR(JabberClient, 512);
IMPLEMENT_ALLOCATOR(JabberClient);

/*! \brief The signal handler */
void handle_signal(int signal) {
    log(INFO, "signal caught");
    running = 0;
}

/*! \brief Return the time remaning to the closest possible timeout  */
time_type jb_closest_timeout(JabberBind* bind) {
    list_iterator it;
    JabberClient* j_client;
    time_type closest, tmp, current;

    closest = bind->session_timeout;
    current = get_time();

    /* check each connection */
    list_foreach(it, bind->jabber_connections) {
        j_client = list_iterator_value(it);
        if(j_client->connection != NULL) {
            /* we have a request, so the timeout is the request timeout */
            tmp = (j_client->timestamp + j_client->wait) - current;
        } else {
            /* we don't have a request, so the timeout is the sessin timeout */
            tmp = (j_client->timestamp + bind->session_timeout) - current;
        }
        if(tmp < closest) {
            closest = tmp;
        }
    }

    return closest;
}

/*! \brief Flush pending messages to the client */
void jc_flush_messages(JabberClient* j_client) {
    char* xml;
    char* body;
    char* buffer;
    char* ptr;
    iks* msg;
    list* xmls;
    int size, n;

    /* check if there is a pending request and if there is any data to send */
    if(j_client->connection != NULL && !list_empty(j_client->output_queue)) {

        size = 0;
        xmls = list_new();

        /* foreach message, convert to string and sum the size */
        while(!list_empty(j_client->output_queue)) {
            msg = list_pop_front(j_client->output_queue);
            xml = iks_string(NULL, msg);
            list_push_back(xmls, xml);
            size += strlen(xml);
            iks_delete(msg);
        }

        /* alloc a buffer */
        ptr = buffer = malloc(size + 1);

        /* put the message in the buffer */
        while(!list_empty(xmls)) {
            xml = list_pop_front(xmls);
            n = strlen(xml);
            memcpy(ptr, xml, n);
            ptr += n;
            iks_free(xml);
        }
        *ptr = 0;
        list_delete(xmls, NULL);

        /* create http content */
        asprintf(&body, MESSAGE_WRAPPER, buffer);

        log(INFO, "%s", body);

        /* send messages */
        hs_answer_request(j_client->connection, body, strlen(body));
        j_client->connection = NULL;

        /* update last activity */
        j_client->timestamp = get_time();

        free(buffer);
        free(body);
    }
}

/*! \brief answer a request with an empty body */
void jc_drop_request(JabberClient* j_client, int terminate) {
    char* body;

    if(terminate) {
        asprintf(&body, TERMINATE_SESSIN_RESPONSE);
    } else {
        asprintf(&body, EMPTY_RESPONSE);
    }
    hs_answer_request(j_client->connection, body, strlen(body));
    j_client->connection = NULL;
    j_client->timestamp = get_time();
    free(body);
}

/*! \brief Put a message to be sent to a client */
void jc_queue_message(JabberClient* j_client, iks* message) {
    list_push_back(j_client->output_queue, message);
    jc_flush_messages(j_client);
}

/*! \brief Free an iks struct */
static void _iks_delete(void* _iks) {
    iks* iks = _iks;
    iks_delete(iks);
}

/*! \brief Close a connection to the jabber server */
void jb_close_client(JabberClient* j_client) {
    JabberBind* bind = j_client->bind;

    log(INFO, "sid = %" PRId64, j_client->sid);

    /* drop the request if we have one */
    if(j_client->connection != NULL) {
        jc_drop_request(j_client, 1);
    }

    /* stop monitoring the socket */
    sm_remove_socket(bind->monitor, j_client->socket_fd);

    /* free iks struct */
    iks_disconnect(j_client->parser);
    iks_parser_delete(j_client->parser);

    /* clsoe the socket */
    close(j_client->socket_fd);

    /* erase the client from the list of clients */
    list_erase(j_client->it);

    /* erase the client's sid */
    uint64_hash_erase(bind->sids, j_client->sid);

    /* free client struct */
    list_delete(j_client->output_queue, _iks_delete);
    JabberClient_free(j_client);
}

/*! \brief Check timeouts and handle them */
void jb_check_timeout(JabberBind* bind) {
    JabberClient* j_client;
    list_iterator it;
    time_type init, idle;
    list* to_close;

    /* list of connections that are to be closed */
    to_close = list_new();

    /* get current time */
    init = get_time();

    list_foreach(it, bind->jabber_connections) {
        j_client = list_iterator_value(it);

        idle = init - j_client->timestamp;
        if(j_client->connection != NULL && idle >= j_client->wait) {
            /* drop the request if it timed out */
            jc_drop_request(j_client, 0);
        } else if(j_client->connection == NULL && idle >= bind->session_timeout) {
            /* close the conenction if the session timedout due to innactivity */
            log(INFO, "timeout on sid = %" PRId64, j_client->sid);
            list_push_back(to_close, j_client);
        }
    }

    /* close connections */
    while(!list_empty(to_close)) {
        j_client = list_pop_front(to_close);
        jb_close_client(j_client);
    }

    /* free local allocs */
    list_delete(to_close, NULL);
}

/*! Handle an incoming message from the jabber server */
int jc_handle_stanza(void* _j_client, int type, iks* stanza) {
    JabberClient* j_client = _j_client;
    /* no message ? */
    if(stanza == NULL) {
        return IKS_OK;
    }
    if(type == IKS_NODE_NORMAL) {
        /* queue up a normal message */
        jc_queue_message(j_client, stanza);
    } else if(type == IKS_NODE_ERROR || type == IKS_NODE_STOP) {
        /* close the connection in case of error or stop */
        log(INFO, "jabber connection ended");
        iks_delete(stanza);
        j_client->alive = 0;
    } else {
        /* ignore otherwise */
        iks_delete(stanza);
    }
    return IKS_OK;
}

/*! \brief Handle activity in the jabber connection */
void jc_read_jabber(void* _j_client) {
    int ret;
    JabberClient* j_client = _j_client;

    /* receive messages, messages will be handled to jc_handle_stanza */
    ret = iks_recv(j_client->parser, 0);

    /* close the client if the connection was closed */
    if(j_client->alive == 0 || ret != IKS_OK) {
        jb_close_client(j_client);
    }
}

/* \brief Report a error to the client */
void jc_report_error(HttpConnection* connection, enum BIND_ERROR_CODE code) {
    char* body;

    /* create the message */
    asprintf(&body, ERROR_RESPONSE, ERROR_TABLE[code][0], ERROR_TABLE[code][1]);

    /* send the message */
    hs_answer_request(connection, body, strlen(body));

    /* free local stuff */
    free(body);
}

uint64_t gen_sid() {
    return lrand48() | (((uint64_t)lrand48())<<32);
}

/*! \brief Create a new connection to the jabber server */
JabberClient* jb_connect_client(JabberBind* bind, HttpConnection* connection, iks* body) {
    char* tmp;
    char* host;
    char* bind_body;
    iksparser* parser;
    JabberClient* j_client;
    uint64_t rid;

    /* alloc memory */
    j_client = JabberClient_alloc();

    /* get wait parameter */
    tmp = iks_find_attrib(body, "wait");
    if(tmp == NULL) {
        j_client->wait = bind->default_request_timeout;
    } else {
        j_client->wait = atoi(tmp) * 1000;
    }

    /* get to parameter */
    host = iks_find_attrib(body, "to");
    if(host == NULL) {
        log(WARNING, "wrong header");
        JabberClient_free(j_client);
        iks_delete(body);
        jc_report_error(connection, BAD_FORMAT);
        return NULL;
    }

    /* get rid parameter */
    tmp = iks_find_attrib(body, "rid");
    if(tmp == NULL) {
        log(WARNING, "Wrong header");
        JabberClient_free(j_client);
        iks_delete(body);
        jc_report_error(connection, BAD_FORMAT);
        return NULL;
    }
    sscanf(tmp, "%" PRId64, &rid);

    /* connect to the jabber server */
    parser = jabber_connect(host, bind->jabber_port, j_client, jc_handle_stanza);
    if(parser == NULL) {
        log(ERROR, "could not connect to jabber server");
        JabberClient_free(j_client);
        iks_delete(body);
        jc_report_error(connection, CONNECTION_FAILED);
        return NULL;
    }

    log(INFO, "connected to jabber server");

    /* pick a random sid */
    do {
        j_client->sid = gen_sid();
    } while(uint64_hash_has_key(bind->sids, j_client->sid));

    /* insert the sid value into the hash */
    uint64_hash_insert(bind->sids, j_client->sid, j_client);

    /* init client values */
    j_client->parser = parser;
    j_client->output_queue = list_new();
    j_client->bind = bind;
    j_client->connection = NULL;
    j_client->alive = 1;
    j_client->timestamp = get_time();
    j_client->socket_fd = iks_fd(j_client->parser);
    j_client->it = list_push_back(bind->jabber_connections, j_client);

    /* start monitoring the socket */
    sm_add_socket(bind->monitor, j_client->socket_fd, jc_read_jabber, j_client);

    /* send request response */
    asprintf(&bind_body, SESSION_RESPONSE, j_client->sid);
    hs_answer_request(connection, bind_body, strlen(bind_body));

    /* free local stuff */
    free(bind_body);
    iks_delete(body);

    return j_client;
}

/*! \brief Find a jabber client by its sid */
JabberClient* jb_find_client(JabberBind* bind, uint64_t sid) {
    return uint64_hash_find(bind->sids, sid);
}

/*! \brief Set the client request */
void jc_set_http(JabberClient* j_client, HttpConnection* connection, uint64_t rid) {
    /* if we already have one, drop it */
    if(j_client->connection != NULL) {
        jc_drop_request(j_client, 0);
    }

    /* update values */
    j_client->connection = connection;
    j_client->timestamp = get_time();
    j_client->rid = rid;

    /* flush messages */
    jc_flush_messages(j_client);
}

/*! \brief Handle an incoming request */
void jb_handle_request(void* _bind, const HttpRequest* request) {
    JabberBind* bind;
    JabberClient* j_client;
    iks* message, *stanza;
    char* tmp;
    uint64_t sid, rid;

    bind = _bind;

    log(INFO, "%s", request->data);

    /* parse the content */
    message = iks_tree(request->data, request->data_size, NULL);

    /* return a error if the xml is malformed */
    if(message == NULL) {
        jc_report_error(request->connection, BAD_FORMAT);
        return;
    }

    /* get the sid */
    tmp = iks_find_attrib(message, "sid");
    if(tmp == NULL) {
        /* if there is no sid, than it is a request to create a connection */
        jb_connect_client(bind, request->connection, message);
    } else {
        /* parse the sid */
        sscanf(tmp, "%" PRId64, &sid);

        /* get the rid */
        tmp = iks_find_attrib(message, "rid");
        if(tmp == NULL) {
            log(WARNING, "rid not found");
            jc_report_error(request->connection, BAD_FORMAT);
            iks_delete(message);
            return;
        }
        sscanf(tmp, "%" PRId64, &rid);

        /* get the client */
        j_client = jb_find_client(bind, sid);
        if(j_client == NULL) {
            log(WARNING, "sid not found");
            jc_report_error(request->connection, SID_NOT_FOUND);
            iks_delete(message);
            return;
        }
        jc_set_http(j_client, request->connection, rid);

        /* send stanzas to the client */
        for(stanza = iks_first_tag(message); stanza != NULL; stanza = iks_next_tag(stanza)) {
            tmp = iks_string(NULL, stanza);
            log(INFO, "%s", tmp);
            send(j_client->socket_fd, tmp, strlen(tmp), 0);
            iks_free(tmp);
        }

        /* close the connection if the type is terminate */
        if(iks_strcmp(iks_find_attrib(message, "type"), "terminate") == 0) {
            jb_close_client(j_client);
        }

        iks_delete(message);
    }
}

/*! \brief Run the server until a SIGINT or SIGTERM signal is caught */
void jb_run(JabberBind* bind) {
    time_type max_time;

    /* init running */
    running = 1;


    /* set signal handlers */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    log(INFO, "server started");

    while(running == 1) {
        /* take the closets timeout */
        max_time = jb_closest_timeout(bind);

        /* some sanity test */
        if(max_time < 0)
            max_time = 0;

        /* wait at most max_time */
        sm_poll(bind->monitor, max_time);

        /* check if any timeout went off */
        jb_check_timeout(bind);
    }
}

/* !\brief crete a new bind server */
JabberBind* jb_new(iks* config) {
    JabberBind* jb;
    iks* bind_config;
    iks* http_config;
    iks* log_config;
    const char* str;
    int i;

    jb = malloc(sizeof(JabberBind));

    /* Load config */
    bind_config = iks_find(config, "bind");
    http_config = iks_find(config, "http_server");
    log_config = iks_find(config, "log");

    if(bind_config == NULL || http_config == NULL || log_config == NULL) {
        fprintf(stderr, "Incomplete config file.\n");
        free(jb);
        return NULL;
    }

    /* set jabber port */
    if((str = iks_find_attrib(bind_config, "jabber_port")) != NULL) {
        jb->jabber_port = atoi(str);
    } else {
        jb->jabber_port = JABBER_PORT;
    }

    /* set session timeout */
    if((str = iks_find_attrib(bind_config, "session_timeout")) != NULL) {
        jb->session_timeout = atoi(str);
    } else {
        jb->session_timeout = SESSION_TIMEOUT;
    }

    /* set default_request_timeout */
    if((str = iks_find_attrib(bind_config, "default_request_timeout")) != NULL) {
        jb->default_request_timeout = atoi(str);
    } else {
        jb->default_request_timeout = DEFAULT_REQUEST_TIMEOUT;
    }

    /* set log output */
    if(log_config && (str = iks_find_attrib(log_config, "filename")) != NULL) {
        log_set_file(str);
    }

    /* set verbose level */
    if(log_config && (str = iks_find_attrib(log_config, "verbose")) != NULL) {
        if(strcmp(str, "ERROR") == 0) {
            i = ERROR;
        } else if(strcmp(str, "WARNING") == 0) {
            i = ERROR;
        } else if(strcmp(str, "INFO") == 0) {
            i = INFO;
        } else if(strcmp(str, "DEBUG") == 0) {
            i = DEBUG;
        }  else {
            i = ERROR;
        }
        log_set_verbose(i);
    }

    /* create a socket monitor */
    jb->monitor = sm_new();

    /* create the http server */
    jb->server = hs_new(http_config, jb->monitor, jb_handle_request, jb);

    if(jb->server == NULL) {
        sm_delete(jb->monitor);
        free(jb);
        return NULL;
    }

    /* set other values */
    jb->jabber_connections = list_new();
    jb->sids = uint64_hash_new();

    /* seed the random generator */
    srand48(get_time());

    return jb;
}

/*! \brief destroy a bin server*/
void jb_delete(JabberBind* bind) {
    JabberClient* j_client;

    /* close all jabber connections */
    while(!list_empty(bind->jabber_connections)) {
        j_client = list_front(bind->jabber_connections);
        jb_close_client(j_client);
    }

    /* free all data structures */
    list_delete(bind->jabber_connections, NULL);
    uint64_hash_delete(bind->sids);

    /* delete the http server */
    hs_delete(bind->server);

    /* delete the socket monitor */
    sm_delete(bind->monitor);

    free(bind);
}

