#ifndef HEADERS_H
#define HEADERS_H 1

#include <sys/uio.h>
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

struct response_header {
    int status_code;
    char *reason;
    char *content_type;
};

size_t response_header_write(
        struct response_header *header,
        struct iovec *vec);

int request_header_parse(struct request_header *header, char *buff, size_t buff_size);
#endif
