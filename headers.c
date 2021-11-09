#include "headers.h"

int response_header_send(struct response_header *header, int fd) {
    const int buff_size = HEADER_BUFF_SIZE;
    char buff[buff_size];
    char *msg = "OK";
    if(header->reason)
        msg = header->reason;
    if(!header->content_type) {
        header->content_type = "application/octet-stream";
    }

    int written = snprintf(
            buff, buff_size, "HTTP/1.1 %3d %s"CRLF, header->status_code, msg);
    send(fd, buff, written, MSG_MORE);
    written = snprintf(buff, buff_size, "Content-Type: %s" CRLF,
                       header->content_type);
    send(fd, buff, written, MSG_MORE);
    return 0;
}

void header_close(int fd) {
    send(fd, CRLF, sizeof(CRLF)-1, 0);
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
