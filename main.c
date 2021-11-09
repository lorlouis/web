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

#include <assert.h>

#include "headers.h"
#include "mimes.h"

#define BUFFSIZE 4096

/* hashmap containing the mimes (initialized in main) */
struct hmap mimes_hmap = {0};
/* the base directory of the server (supplied by argv[2] */
const char *basedir;
size_t basedir_len;

const char I_AM_A_TEAPOT[] = (
    "<!DOCTYPE html>"
        "<html>"
            "<head>"
                "<title>teapot</title>"
            "</head>"
            "<body>"
                "<h1 style=\"text-align: center;\">I'm a teapot</h1>"
                "<h1 style=\"text-align: center;\">"
                    "<code><strong>{(&macr;)/&acute;</strong></code>"
                "</h1>"
                "<h3 style=\"text-align: center;\">"
                    "Will you have a cup of Tea?"
                "</h3>"
            "</body>"
        "</html>"
);
const size_t I_AM_A_TEAPOT_LEN = sizeof(I_AM_A_TEAPOT);

const char NOT_FOUND_PAGE[] = (
    "<!DOCTYPE html>"
        "<html>"
            "<head>"
                "<title>404</title>"
            "</head>"
            "<body>"
                "<h1>NOT_FOUND</h1>"
                "<p>The resource asked for could not be found</p>"
            "</body>"
        "</html>"
);
const size_t NOT_FOUND_PAGE_LEN = sizeof(NOT_FOUND_PAGE);

const char UNIMPLEMENTED_PAGE[] = (
    "<!DOCTYPE html>"
        "<html>"
            "<head>"
                "<title>405</title>"
            "</head>"
            "<body>"
                "<h1>UNIMPLEMENTED</h1>"
                "<p>this server only implements http GET</p>"
            "</body>"
        "</html>"
);
const size_t UNIMPLEMENTED_PAGE_LEN = sizeof(UNIMPLEMENTED_PAGE);

/* Tries to send data_size from data into sock
 * Returns
 *  the size sent
 *  -1 on fail, check errno */
size_t send_str(
        int code, char *msg,
        const char *mime,
        const char *data, size_t data_size,
        int sock) {

    struct response_header response;
    if(!mime) {
        response.content_type = "text/html";
    }
    response.status_code = code;
    response.reason = msg;

    /* send the header */
    response_header_send(&response, sock);
    header_close(sock);

    /* TODO handle err on write */
    return write(sock, data, data_size);
}

/* Tries to send count char of fd
 * Returns:
 *  the size sent
 *  -1 on fail, check errno */
size_t send_file(
        int code, char *msg,
        char *mime,
        int fd, size_t count,
        int sock) {

    struct response_header response;
    response.status_code = code;
    response.reason = msg;
    response.content_type = mime;

    response_header_send(&response, sock);
    header_close(sock);

    /* send a file */
    return sendfile(sock, fd, 0, count);
}

/* Tries to send a whole file
 * Returns:
 *  the size sent
 *  -1 on fail, check errno */
size_t send_whole_file(
        int code, char *msg,
        char *mime,
        int fd, int sock) {
    /* send a file */
    struct stat stat;
    if(fstat(fd, &stat) == -1)
        return -1;

    return send_file(code, msg, mime, fd, stat.st_size, sock);
}

int send_404(int sock) {
    return send_str(
            404, "page not found", 0,
            NOT_FOUND_PAGE, NOT_FOUND_PAGE_LEN,
            sock);
}

int send_405(int sock) {
    return send_str(
            405, "this server only supports http GET", 0,
            NOT_FOUND_PAGE, NOT_FOUND_PAGE_LEN,
            sock);
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


int handle_conn(int sock) {
    /* depends on basedir, basedir_len and mimes_hmap */
    char buff[BUFFSIZE]={0};
    char path_buff[BUFFSIZE]={0};
    int file=-1;

    struct request_header request = {0};
    char index[] = "index.html";

    size_t file_len = 0;

    char *extension = 0;
    char *type = 0;

    const char html_begin[] = "<!DOCTYPE html>";
    const size_t html_begin_size = sizeof(html_begin);
    static_assert(sizeof(html_begin) == 16, "weird compiler");

    memcpy(path_buff, basedir, basedir_len);
    path_buff[basedir_len] = '/';


    /* nothing to read */
    if(!read(sock, buff, BUFFSIZE)) {
        close(sock);
        return -1;
    }

    printf("%s\n", buff);

    /* check if the content isn't GET */
    if(strncmp(buff, "GET ", 4)) {
        /* unsuported protocol */
        send_405(sock);
        close(sock);
        return 0;
    }

    request_header_parse(&request, buff, BUFFSIZE);

    request.file++;
    file_len = strlen(request.file);
    /* FIXME temporary workaroud */
    if(file_len == 0) {
        request.file = index;
        file_len = 10;
    }

    memcpy(path_buff + basedir_len + 1, request.file, file_len);
    path_buff[basedir_len + 1 + file_len] = '\0';

    /* extract MIME information */
    strtok(request.file, ".");
    extension = strtok(0, "/");
    type = 0;

    /* open the file */
    file = open(path_buff, O_RDONLY);

    /* file not found */
    if(file == -1) {
        /* check for the very important teapot */
        if(!strcmp(request.file, "teapot")) {
            send_str(418, "I'm a tea pot", 0,
                     I_AM_A_TEAPOT, I_AM_A_TEAPOT_LEN,
                     sock);
            close(sock);
            return 0;
        }
        /* return a boring old 404 */
        send_404(sock);
        close(sock);
        return 0;
    }
    /* ##### At this point a file is found ##### */
    /* check for the mimetype in the hashmap */
    if(extension) {
        type = hmap_get(&mimes_hmap, extension);
    }
    if(!type) {
        type = "application/octet-stream";
        /* support for untagged html pages */
        if(read(file, buff, html_begin_size-1) == html_begin_size-1
           && !strncmp(buff, html_begin, html_begin_size-1)) {
            type = "text/html";
        }
        lseek(file, 0, SEEK_SET);
    }

    /* send the file */
    send_whole_file(200, 0, type, file, sock);
    close(file);
    close(sock);
    return 0;
}


int main(int argc, const char **argv) {
    int port_no;
    struct sockaddr_in serv_addr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    int serv_fd;

    /* check params */
    if(argc == 3){
        errno = 0;
        int tmp = (int)strtoul(argv[1], NULL, 10);
        if(errno == EINVAL || !(tmp > 0 && tmp <= 65535)) {
            fprintf(stderr,
                    "port(%s) must be an integer between 1 and 65535\n",
                    argv[1]);
            return -1;
        };
        basedir = argv[2];
        basedir_len = strlen(basedir);
        port_no = tmp;
    }
    else {
        fprintf(stderr, "Usage: sv <port> <base dir>\nport needs"
                        " to be between 1 and 65535 inclusively\n");
        return -1;
    }

    puts("Building MIME type hashmap");
    /* initialise the mime hashmap */
    build_mimes_hmap(&mimes_hmap);

    printf("starting server on 0.0.0.0:%d\n", port_no);
#ifndef URING
    /* setup socket for listen */
    if(serv_setup(port_no, &serv_fd, &serv_addr)) {
        perror("");
        return -1;
    }
    printf("started!\n");
    /* ###########################Start Serving############################# */
    /* server loop */
    for(;;) {
        int new_fd = accept(serv_fd, (struct sockaddr*)&serv_addr, &socklen);
        if(handle_conn(new_fd)) {
            puts("An error occured on a connection (empty read)");
        }
    }
    /* close the socket */
    close(serv_fd);
#else

#endif

    return 0;
}
