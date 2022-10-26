#include "response_header.h"

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
}

size_t response_header_write(
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
    for(int i = 0; i < header->arbitrary.len; i++) {
        printf("len: %ld\n", header->arbitrary.len);
        printf("cap: %ld\n", header->arbitrary.capacity);
        struct key_value *kv = kv_vec_getref(
                &header->arbitrary,
                i);
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
