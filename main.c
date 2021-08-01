#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define BUFFSIZE 4096

#define CRLF "\xd\xa"

void panic(char* str) {
    perror(str);
    exit(EXIT_FAILURE);
}

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
    char extension_code[3];
    char *reason;
    char *content_type;
};

int send_response_header(struct response_header *header, int fd) {
    const int buff_size = 512;
    char buff[buff_size];
    char *msg = "OK";
    if(header->status_code != 200)
        msg = header->reason;

    int written = snprintf(
            buff, buff_size, "HTTP/1.1 %3d %s"CRLF, header->status_code, msg);
    send(fd, buff, written, MSG_MORE);
    written = snprintf(buff, buff_size, "Content-Type: %s"CRLF, header->content_type);
    send(fd, buff, written, MSG_MORE);
    return 0;
}

void close_header(int fd) {
    send(fd, CRLF, sizeof(CRLF)-1, 0);
}

int parse_request(struct request_header *header, char *buff, size_t buff_size){
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
    line = strtok(0, "\r\n");

    char *remainder = 0;
    header->version = strtof(line, &remainder);
    /* malformed HTML/1.1 or whatever */
    if(line == remainder) return 1;

    return 0;
}

int main(int argc, const char **argv) {
    int sock_fd, port_no = 80, opt = 1;
    struct sockaddr_in serv_addr;
    socklen_t socklen = sizeof(serv_addr);
    char buff[BUFFSIZE];

    if(argc == 2){
        errno = 0;
        int tmp = (int)strtoul(argv[1], NULL, 10);
        if(errno) panic(0);
        if(tmp > 0 && tmp <= 65535) port_no = tmp;
        else {
            errno = 33;  /* EDOM */
            panic("usage: sv <port>\nport needs to be between 1 and 65535"
                  "inclusively\n");
        }
    }

    /* AF_INET means web, sockstream means like a file
     * 0 asks the kernel to chose the protocol TCP in this case */
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        panic("Could not open socket");
    if((setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int))) < 0)
        panic("Could not reuse socket");

    memset(&serv_addr, 0, sizeof(serv_addr));

    /* internet socket */
    serv_addr.sin_family = AF_INET;
    /* convert host byte order to network byte order (short) */
    serv_addr.sin_port = htons(port_no);
    /* address on the server */
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if((bind(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0)
        panic("Could not bind address\n");
    if(listen(sock_fd, 20) != 0)
        panic("Could not set the socket in passive mode\n");
    /* ready to start serving */

    while(1) {
        int file;
        int new_fd = accept(sock_fd, (struct sockaddr*)&serv_addr, &socklen);
        struct request_header request = {0};
        struct response_header response = {0};

        read(new_fd, buff, BUFFSIZE);


        parse_request(&request, buff, BUFFSIZE);

        response.status_code = 200;
        response.content_type = "text/html;";

        request.file++;
        if(!strncmp(request.file, "", 2)) {
            file = open("index.html", O_RDONLY);
        }
        else {
            file = open(request.file, O_RDONLY);
        }

        if(file == -1) {
            response.status_code = 404;
            response.reason = "page not found";
        }

        /* send the header */
        send_response_header(&response, new_fd);
        close_header(new_fd);

        struct stat stat;
        fstat(file, &stat);
        sendfile(new_fd, file, 0, stat.st_size);

        printf("%s\n", request.file);


        close(file);
        close(new_fd);
    }
    close(sock_fd);

    return 0;
}
