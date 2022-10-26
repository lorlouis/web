#ifndef HEADERS_H
#define HEADERS_H 1

#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>

#define CRLF "\xd\xa"

#define HEADER_BUFF_SIZE 512

enum http_method {
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    CONNECT,
    OPTIONS,
    TRACE,
    PATCH
};

struct request_header {
    enum http_method metod;
    float version;
    char *file;
    char *host;
    char *user_agent;
};

struct key_value {
    char *key;
    char *value;
};

/* returns the length written */
int request_header_parse(struct request_header *header, char *buff, size_t buff_size);
#endif
