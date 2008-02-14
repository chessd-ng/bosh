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

#include "http_server.h"
#include "jabber_bind.h"
#include "jabber.h"
#include "log.h"

#define JABBER_PORT 5222

#define SESSION_TIMEOUT (30000)

#define DEFAULT_REQUEST_TIMEOU (20000)

#define BIND_BODY "<body rid='%d' sid='%d' xmlns='http://jabber.org/protocol/httpbind'>" \
                    "%s" \
                    "</body>"

#define BIND_BODY_TYPE "<body rid='%d' sid='%d' type='%s' xmlns='http://jabber.org/protocol/httpbind'>" \
                    "%s" \
                    "</body>"

#define JABBER_ERROR_MSG "<error>%s</error>"

int running;

void handle_signal(int signal) {
    log("signal caught");
    running = 0;
}


static int compare_sid(void* s1, void* s2) {
	return *(int*)s1 == *(int*)s2;
}

static unsigned int hash_sid(void* s) {
	return *(int*)s;
}

time_type jb_closest_timeout(JabberBind* bind) {
    list_iterator it;
    JabberClient* j_client;
    time_type closest = bind->session_timeout, tmp, current;

    current = get_time();

	list_foreach(it, bind->jabber_connections) {
        j_client = list_iterator_value(it);
		if(j_client->connection != NULL) {
			tmp = (j_client->timestamp + j_client->wait) - current;
		} else {
			tmp = (j_client->timestamp + bind->session_timeout) - current;
		}
        if(tmp < closest) {
            closest = tmp;
        }
    }

    return closest;
}

void jc_flush_messages(JabberClient* j_client) {
	char* xml;
	char* body;
	char* buffer;
	char* ptr;
	iks* msg;
	list* xmls;
	int size, n;

    if(j_client->connection != NULL && !list_empty(j_client->output_queue)) {
		size = 0;
		xmls = list_new();

		while(!list_empty(j_client->output_queue)) {
			msg = list_pop_front(j_client->output_queue);
			xml = iks_string(NULL, msg);
			list_push_back(xmls, xml);
			size += strlen(xml);
			iks_delete(msg);
		}

		ptr = buffer = malloc(size + 1);

		while(!list_empty(xmls)) {
			xml = list_pop_front(xmls);
			n = strlen(xml);
			memcpy(ptr, xml, n);
			ptr += n;
			iks_free(xml);
		}
		*ptr = 0;
		list_delete(xmls, NULL);

        asprintf(&body, BIND_BODY, j_client->rid, j_client->sid, buffer);

		log(body);

		hs_answer_request(j_client->connection, body, strlen(body));
		j_client->connection = NULL;

		free(buffer);
		free(body);
		j_client->timestamp = get_time();
    }
}

void jc_drop_request(JabberClient* j_client) {
	char*body;

	asprintf(&body, BIND_BODY, j_client->rid, j_client->sid, "");
    hs_answer_request(j_client->connection, body, strlen(body));
    j_client->connection = NULL;
    j_client->timestamp = get_time();
	free(body);
}

void jc_queue_message(JabberClient* j_client, iks* message) {
    list_push_back(j_client->output_queue, message);
    jc_flush_messages(j_client);
}

static void _iks_delete(void* _iks) {
    iks* iks = _iks;
    iks_delete(iks);
}

void jb_close_client(JabberClient* j_client) {
    JabberBind* bind = j_client->bind;

    log("sid = %d", j_client->sid);

    if(j_client->connection != NULL) {
        jc_drop_request(j_client);
    }

    sm_remove_socket(bind->monitor, j_client->socket_fd);

    iks_disconnect(j_client->parser);
    iks_parser_delete(j_client->parser);

    close(j_client->socket_fd);

	list_erase(j_client->it);

	hash_erase(bind->sid_hash, &j_client->sid);

    list_delete(j_client->output_queue, _iks_delete);
    free(j_client);
}

void jb_check_timeout(JabberBind* bind) {
    JabberClient* j_client;
    list_iterator it;
    time_type init, idle;
    list* to_close;
    
    to_close = list_new();

    init = get_time();

	list_foreach(it, bind->jabber_connections) {
        j_client = list_iterator_value(it);

		idle = init - j_client->timestamp;
        if(j_client->connection != NULL && idle >= j_client->wait) {
			jc_drop_request(j_client);
		} else if(j_client->connection == NULL && idle >= bind->session_timeout) {
			log("timeout on sid = %d", j_client->sid);
			list_push_back(to_close, j_client);
		}
    }

    while(!list_empty(to_close)) {
        j_client = list_pop_front(to_close);
        jb_close_client(j_client);
    }

    list_delete(to_close, NULL);
}

int jc_handle_stanza(void* _j_client, int type, iks* stanza) {
    JabberClient* j_client = _j_client;
    if(stanza == NULL) {
        return IKS_OK;
    }
    if(type == IKS_NODE_NORMAL /*&& strcmp(iks_name(stanza), "stream:features") != 0*/) {
        jc_queue_message(j_client, stanza);
    } else if(type == IKS_NODE_ERROR || type == IKS_NODE_STOP) {
        log("jabber connection ended");
        iks_delete(stanza);
        j_client->alive = 0;
    } else {
        iks_delete(stanza);
    }
    return IKS_OK;
}

void jc_read_jabber(void* _j_client) {
    JabberClient* j_client = _j_client;
    iks_recv(j_client->parser, 0);
    if(j_client->alive == 0)
        jb_close_client(j_client);
}

void jc_report_error(HttpConnection* connection, const char* msg) {
	char* body;

	asprintf(&body, JABBER_ERROR_MSG, msg);

	hs_answer_request(connection, body, strlen(body));

	free(body);
}

JabberClient* jb_connect_client(JabberBind* bind, HttpConnection* connection, iks* body) {
    char* tmp;
    char* host;
	char* bind_body;
	iksparser* parser;
    JabberClient* j_client;
	int rid;
	
	j_client = malloc(sizeof(JabberClient));

    tmp = iks_find_attrib(body, "wait");
    if(tmp == NULL) {
        j_client->wait = bind->default_request_timeout;
    } else {
        j_client->wait = atoi(tmp) * 1000;
    }

    host = iks_find_attrib(body, "to");
    if(host == NULL) {
        log("wrong header");
        free(j_client);
		iks_delete(body);
		jc_report_error(connection, "Invalid xml header.");
        return NULL;
    }

    tmp = iks_find_attrib(body, "rid");
    if(tmp == NULL) {
        log("Wrong header");
        free(j_client);
		iks_delete(body);
		jc_report_error(connection, "No rid found.");
        return NULL;
    }
	rid = atoi(tmp);

	parser = jabber_connect(host, bind->jabber_port, j_client, jc_handle_stanza);
	if(parser == NULL) {
        log("could not connect to jabber server");
        free(j_client);
		iks_delete(body);
		jc_report_error(connection, "Could not connect to the jabber server.");
        return NULL;
	}

    log("connected to jabber server");

	do {
		j_client->sid = lrand48();
	} while(hash_has_key(bind->sid_hash, &j_client->sid));

    j_client->parser = parser;
    j_client->output_queue = list_new();
    j_client->bind = bind;
    j_client->connection = NULL;
    j_client->alive = 1;
    j_client->timestamp = get_time();
    j_client->socket_fd = iks_fd(j_client->parser);
	j_client->it = list_push_back(bind->jabber_connections, j_client);

    sm_add_socket(bind->monitor, j_client->socket_fd, jc_read_jabber, j_client);
	hash_insert(bind->sid_hash, &j_client->sid, j_client);
    
    asprintf(&bind_body, BIND_BODY, rid, j_client->sid, "");
    hs_answer_request(connection, bind_body, strlen(bind_body));
	free(bind_body);

    iks_delete(body);

    return j_client;
}

JabberClient* jb_find_jabber(JabberBind* bind, int sid) {
    return hash_find(bind->sid_hash, &sid);
}

void jc_set_http(JabberClient* j_client, HttpConnection* connection, int rid) {
    if(j_client->connection != NULL) {
        jc_drop_request(j_client);
    }

    j_client->connection = connection;
    j_client->timestamp = get_time();
	j_client->rid = rid;

    jc_flush_messages(j_client);
}

void jc_deliver_message(JabberClient* j_client, iks* message) {
    iks* stanza;
    char* xml;

    for(stanza = iks_first_tag(message); stanza != NULL; stanza = iks_next_tag(stanza)) {
        xml = iks_string(NULL, stanza);
        log(xml);
        send(j_client->socket_fd, xml, strlen(xml), 0);
        iks_free(xml);
    }
    iks_delete(message);
}

void jb_handle_request(void* _bind, const HttpRequest* request) {
	JabberBind* bind;
	JabberClient* j_client;
	iks* message;
	char* tmp;
	int sid, rid;

	bind = _bind;

	log(request->data);

	message = iks_tree(request->data, request->data_size, NULL);

	if(message == NULL) {
		jc_report_error(request->connection, "malformed xml");
		return;
	}

	tmp = iks_find_attrib(message, "sid");
	if(tmp == NULL) {
		jb_connect_client(bind, request->connection, message);
	} else {
		sid = atoi(tmp);
		tmp = iks_find_attrib(message, "rid");
		if(tmp == NULL) {
			log("rid not found");
			jc_report_error(request->connection, "wrong format");
			iks_delete(message);
			return;
		}
		rid = atoi(tmp);
		j_client = jb_find_jabber(bind, sid);
		if(j_client == NULL) {
			log("sid not found");
			jc_report_error(request->connection, "invalid sid");
			iks_delete(message);
			return;
		}
		jc_set_http(j_client, request->connection, rid);

		jc_deliver_message(j_client, message);

        if(iks_strcmp(iks_find_attrib(message, "type"), "terminate") == 0) {
            jb_close_client(j_client);
        }
	}
}

void jb_run(JabberBind* bind) {
	time_type max_time;
    
    running = 1;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    log("server started");
    while(running == 1) {
		max_time = jb_closest_timeout(bind);

		if(max_time < 0)
			max_time = 0;

		sm_poll(bind->monitor, max_time);
		jb_check_timeout(bind);
    }
}

JabberBind* jb_new(iks* config) {
	JabberBind* jb;
    iks* bind_config;
    iks* http_config;
    const char* str;

	jb = malloc(sizeof(JabberBind));

    /* Load config */

    bind_config = iks_find(config, "bind");
    http_config = iks_find(config, "http_server");

    if(bind_config == NULL || http_config == NULL) {
        fprintf(stderr, "Incomplete config file, bind or http_server tag is missing\n");
        free(jb);
        return NULL;
    }

    /* load jabber port */
    if((str = iks_find_attrib(config, "jabber_port")) != NULL) {
        jb->jabber_port = atoi(str);
    } else {
        jb->jabber_port = JABBER_PORT;
    }
    
    /* load session timeout */
    if((str = iks_find_attrib(config, "session_timeout")) != NULL) {
        jb->session_timeout = atoi(str);
    } else {
        jb->session_timeout = SESSION_TIMEOUT;
    }

    /* load default_request_timeout */
    if((str = iks_find_attrib(config, "default_request_timeout")) != NULL) {
        jb->default_request_timeout = atoi(str);
    } else {
        jb->default_request_timeout = DEFAULT_REQUEST_TIMEOU ;
    }

	jb->monitor = sm_new();
	jb->server = hs_new(http_config, jb->monitor, jb_handle_request, jb);

	if(jb->server == NULL) {
		sm_delete(jb->monitor);
		free(jb);
		return NULL;
	}

	jb->jabber_connections = list_new();
	jb->sid_hash = hash_new(hash_sid, compare_sid);

	return jb;
}

void jb_delete(JabberBind* bind) {
	JabberClient* j_client;

	while(!list_empty(bind->jabber_connections)) {
		j_client = list_front(bind->jabber_connections);
		jb_close_client(j_client);
	}

	list_delete(bind->jabber_connections, NULL);
	hash_delete(bind->sid_hash);
	hs_delete(bind->server);
	sm_delete(bind->monitor);

	free(bind);
}

