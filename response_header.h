#ifndef RESPONSE_HEADER_H
#define RESPONSE_HEADER_H 1

#include <sys/uio.h>

#include "headers.h"

struct response_header {
    int status_code;
    char *reason;
    char *content_type;
    struct key_value *arbitrary;
    size_t arbitrary_len;
};

void response_header_init(
        struct response_header *response,
        int code,
        char *msg,
        const char *mime);

size_t response_header_write(
        struct response_header *header,
        struct iovec *vec);
#endif
