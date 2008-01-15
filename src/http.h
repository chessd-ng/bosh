#ifndef HTTP_H
#define HTTP_H

#define MAX_HTTP_FIELDS (64)

#define HTTP_LINE_SEP "\r\n"

typedef struct HttpField {
    char* name;
    char* value;
} HttpField;

typedef struct HttpHeader {
    HttpField fields[MAX_HTTP_FIELDS];
    int n_fields;
} HttpHeader;

void http_delete(HttpHeader* header);

HttpHeader* http_parse(const char* str);

char* make_http_head(int http_code, size_t data_size);

const char* http_get_field(HttpHeader* header, const char* field);

#endif
