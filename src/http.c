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

#include "http.h"

#define HTTP_HEADER "HTTP/1.1 %d %s\r\n" \
                        "Content-type: text/xml; charset=utf-8\r\n" \
                        "Content-Length: %d\r\n" \
                        "\r\n"

void http_delete(HttpHeader* header) {
    int i;

    for(i = 0; i < header->n_fields; ++i) {
        free(header->fields[i].name);
        free(header->fields[i].value);
    }

    free(header);
}

HttpHeader* http_parse(const char* str) {
    const char* end_line;
    const char* colon;
    int fail = 0;
    int i;
    HttpHeader* header;

    if(strstr(str, HTTP_LINE_SEP HTTP_LINE_SEP) == NULL)
        return NULL;
    
    header = malloc(sizeof(HttpHeader));
    header->n_fields = 0;

    for(i = 0; i < MAX_HTTP_FIELDS; ++i) {
        end_line = strstr(str, HTTP_LINE_SEP);
        if(end_line == NULL) {
            fail = 1;
            break;
        }
        if(end_line == str)
            break;
        colon = strstr(str, ":");
        if(colon == NULL) {
            fail = 1;
            break;
        }
        header->fields[i].name = strndup(str, colon - str);
        header->fields[i].value = strndup(colon + 1, end_line - (colon + 1));
        header->n_fields ++;

        str = end_line + 2;
    }
    if(fail) {
        http_delete(header);
        return NULL;
    } else {
        return header;
    }
}

char* make_http_head(int code, size_t data_size) {
	char* msg;
	const char* code_msg;

	if(code == 200) {
		code_msg = "OK";
	} else {
		code_msg = "ERROR";
	}

	asprintf(&msg, HTTP_HEADER, code, code_msg, (int)data_size);

	return msg;
}

const char* http_get_field(HttpHeader* header, const char* field) {
    int i;

    for(i = 0;i < header->n_fields; ++i) {
        if(strcmp(field, header->fields[i].name) == 0)
            return header->fields[i].value;
    }

    return NULL;
}

