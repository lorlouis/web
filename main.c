#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "headers.h"
#include "mimes.h"

#define BUFFSIZE 4096


void panic(char* str) {
    perror(str);
    exit(EXIT_FAILURE);
}

int main(int argc, const char **argv) {
    int sock_fd, port_no = 80, opt = 1;
    struct sockaddr_in serv_addr;
    socklen_t socklen = sizeof(serv_addr);

    /* this is garbage and I want to vomit */
    struct hmap mimes_hmap = {0};
    build_mimes_hmap(&mimes_hmap);

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

    printf("starting server on 0.0.0.0:%d\n", port_no);

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
    if(listen(sock_fd, 256) != 0)
        panic("Could not set the socket in passive mode\n");
    /* ready to start serving */

    printf("started!\n");

    char buff[BUFFSIZE];
    for(;;) {
        int file;
        int new_fd = accept(sock_fd, (struct sockaddr*)&serv_addr, &socklen);
        struct request_header request = {0};
        struct response_header response = {0};

        if(!read(new_fd, buff, BUFFSIZE)) {
            close(new_fd);
            continue;
        }

        printf("%s\n", buff);

        request_header_parse(&request, buff, BUFFSIZE);

        response.status_code = 200;
        response.content_type = "text/html;";

        request.file++;
        if(!strncmp(request.file, "", 2)) {
            file = open("index.html", O_RDONLY);
        }
        else {
            file = open(request.file, O_RDONLY);

            strtok(request.file, ".");
            char *extention = strtok(0, "/");
            char *type = 0;

            if(extention) {
                type = hmap_get(&mimes_hmap, extention);
            }
            if(type) {
                response.content_type = type;
                printf("%s || %s\n", extention, type);
            }
        }

        if(file == -1) {
            response.status_code = 404;
            response.reason = "page not found";
        }


        /* send the header */
        response_header_send(&response, new_fd);
        header_close(new_fd);

        struct stat stat;
        fstat(file, &stat);
        sendfile(new_fd, file, 0, stat.st_size);

        close(file);
        close(new_fd);
    }
    close(sock_fd);

    return 0;
}
