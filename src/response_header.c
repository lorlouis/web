#include <unistd.h>

#include "response_header.h"

VEC_FUNCTIONS(kv_vec, struct key_value);

void response_header_init(
        struct response_header *response,
        int code,
        char *msg,
        const char *mime) {

    response->status_code = code;

    if(!msg) {
        response->reason = "";
    }

    if(!mime) {
        response->content_type = "text/html";
    }
    kv_vec_init(&response->key_values, 0, 0);
}

void response_header_cleanup(struct response_header *header) {
    kv_vec_cleanup(&header->key_values);
}

ssize_t response_header_write(
        struct response_header *header,
        struct iovec *vec) {
    char *msg = "OK";
    int written = 0;
    int ret;

    if(header->reason)
        msg = header->reason;
    if(!header->content_type) {
        header->content_type = "application/octet-stream";
    }
    ret = snprintf(
            vec->iov_base, vec->iov_len,
            "HTTP/1.1 %3d %s"CRLF
            "Content-Type: %s"CRLF,
            header->status_code,
            msg,
            header->content_type);
    if(ret <= 0) return ret;
    written += ret;
    if(written >= vec->iov_len) return -1;

    /* write headers */
    for(int i = 0; i < header->key_values.len; i++) {
        struct key_value *kv = header->key_values.data + i;
        ret = snprintf(
                vec->iov_base + written,
                vec->iov_len - written,
                "%s: %s"CRLF,
                kv->key,
                kv->value);
        if(ret <= 0) return ret;
        written += ret;
        if(written >= vec->iov_len) return -1;
    }

    /* write terminator */
    ret = snprintf(
            vec->iov_base + written,
            vec->iov_len - written,
            CRLF);
    if(ret <= 0) return ret;
    written += ret;

    return written;
}
