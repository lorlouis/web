#include "headers.h"

size_t response_header_write(
        struct response_header *header,
        struct iovec *vec,
        size_t vec_size) {
    char *msg = "OK";
    if(header->reason)
        msg = header->reason;
    if(!header->content_type) {
        header->content_type = "application/octet-stream";
    }

    int written = snprintf(
            vec->iov_base, vec_size,
            "HTTP/1.1 %3d %s"CRLF
            "Content-Type: %s"CRLF"%.*s",
            header->status_code,
            msg,
            header->content_type,
            (int)sizeof(CRLF)-1,
            CRLF);
    return written;
}

int request_header_parse(struct request_header *header, char *buff, size_t buff_size){
    char *line = strtok(buff, " ");
    if(!strcmp(line, "GET"))
        header->metod = GET;
    else if(!strcmp(line, "HEAD"))
        header->metod = HEAD;
    else if(!strcmp(line, "POST"))
        header->metod = POST;
    else if(!strcmp(line, "PUT"))
        header->metod = PUT;
    else if(!strcmp(line, "DELETE"))
        header->metod = DELETE;
    else if(!strcmp(line, "CONNECT"))
        header->metod = CONNECT;
    else if(!strcmp(line, "OPTIONS"))
        header->metod = OPTIONS;
    else if(!strcmp(line, "TRACE"))
        header->metod = TRACE;
    else if(!strcmp(line, "PATCH"))
        header->metod = PATCH;
    header->file = strtok(0, " ");
    strtok(0, "/");
    line = strtok(0, CRLF);

    char *remainder = 0;
    header->version = strtof(line, &remainder);
    /* malformed HTML/1.1 or whatever */
    if(line == remainder) return 1;

    return 0;
}
