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

/* sets up the socket and starts listening on port_no
 * 0 normal
 * 1 err*/
int serv_setup(int port_no, int *sock_fd, struct sockaddr_in *serv_addr) {

    int opt = 1;
    socklen_t socklen = sizeof(*serv_addr);

    /* AF_INET means web, sockstream means like a file
     * 0 asks the kernel to chose the protocol TCP in this case */
    if((*sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return 1;
    if((setsockopt(*sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)))<0) {
        close(*sock_fd);
        return 1;
    }

    memset(serv_addr, 0, socklen);

    /* internet socket */
    serv_addr->sin_family = AF_INET;
    /* convert host byte order to network byte order (short) */
    serv_addr->sin_port = htons(port_no);
    /* address on the server */
    serv_addr->sin_addr.s_addr = INADDR_ANY;

    if((bind(*sock_fd, (struct sockaddr*)serv_addr, socklen)) < 0) {
        close(*sock_fd);
        return 1;
    }
    if(listen(*sock_fd, 256) != 0) {
        close(*sock_fd);
        return 1;
    }
    /* ready to start serving */
    return 0;
}

int main(int argc, const char **argv) {

    /* check params */
    int port_no = 80;
    struct sockaddr_in serv_addr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    const char *basedir;
    size_t basedir_len;

    int serv_fd;

    errno = 0;
    if(argc == 3){
        int tmp = (int)strtoul(argv[1], NULL, 10);
        if(errno == 1 && (tmp > 0 && tmp <= 65535)) {
            fprintf(stderr,
                    "port(%d) must be an integer between 1 and 65535",
                    tmp);
            return -1;
        };
        basedir = argv[2];
        basedir_len = strlen(basedir);
        port_no = tmp;
    }
    else {
        errno = 33;  /* EDOM */
        panic("usage: sv <port> <base dir>\nport needs to be between"
              " 1 and 65535 inclusively\n");
    }

    printf("starting server on 0.0.0.0:%d\n", port_no);
    if(serv_setup(port_no, &serv_fd, &serv_addr)) {
        perror("");
        return -1;
    }

    /* this is garbage and I want to vomit */
    struct hmap mimes_hmap = {0};
    build_mimes_hmap(&mimes_hmap);

    printf("started!\n");

    char buff[BUFFSIZE]={0};
    char path_buff[BUFFSIZE]={0};
    memcpy(path_buff, basedir, basedir_len);
    path_buff[basedir_len] = '/';
    for(;;) {
        int file=-1;
        int new_fd = accept(serv_fd, (struct sockaddr*)&serv_addr, &socklen);
        struct request_header request = {0};
        struct response_header response = {0};
        char index[] = "index.html";

        if(!read(new_fd, buff, BUFFSIZE)) {
            close(new_fd);
            continue;
        }

        printf("%s\n", buff);

        request_header_parse(&request, buff, BUFFSIZE);

        response.status_code = 200;
        response.content_type = "text/html;";

        request.file++;
        size_t file_len = strlen(request.file);
        if(file_len == 0) {
            request.file = index;
            file_len = 10;
        }
        memcpy(path_buff + basedir_len + 1, request.file, file_len);
        path_buff[basedir_len + 1 + file_len] = '\0';



        /* extract MIME information */
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
        /* open the file */
        file = open(path_buff, O_RDONLY);

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
    /* close the socket */
    close(serv_fd);

    return 0;
}
