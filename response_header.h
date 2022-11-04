#ifndef RESPONSE_HEADER_H
#define RESPONSE_HEADER_H 1

#include <sys/uio.h>

#include "headers.h"
#include "vec/vec.h"

VEC(kv_vec, struct key_value);
VEC_PROTYPE(kv_vec, struct key_value);

struct response_header {
    int status_code;
    char *reason;
    const char *content_type;
    struct kv_vec key_values;
};

void response_header_init(
        struct response_header *response,
        int code,
        char *msg,
        const char *mime);

ssize_t response_header_write(
        struct response_header *header,
        struct iovec *vec);
#endif
