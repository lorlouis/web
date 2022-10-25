#include <sys/socket.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <stdbool.h>

#include "headers.h"
#include "mimes.h"
#include "logging.h"
#include "ssl_ex.h"
#include "default_pages.h"

#include "conn.h"

#define BUFFSIZE 4096
#define MAX_BUFF_COUNT_FAST 128
#define ACCEPT_Q_SIZE 256

static volatile bool KEEP_RUNNING = true;

#define MIN(a,b) (a < b ? a : b)

void sigint_halder(int sig) {
    if(sig == SIGINT) {
        /* ignore / acknowledge the signal so that it does not propagate further */
        signal(sig, SIG_IGN);
        puts("Shutting down...");
        KEEP_RUNNING = false;
    }
    /* reinstate the signal handler */
    signal(sig, sigint_halder);
}

/* hashmap containing the mimes (initialized in main) */
struct hmap mimes_hmap = {0};
/* the base directory of the server (supplied by argv[2] */
const char *basedir;
size_t basedir_len;

/* Tries to send data_size from data into sock
 * Returns
 *  the size sent
 *  -1 on fail, check errno */
size_t send_str(
        int code,
        char *msg,
        const char *mime,
        const char *data,
        size_t data_size,
        SSL *sock) {
    struct iovec page[2] = {0};
    struct response_header response;
    size_t ret;

    page[1].iov_base = (void*)data;
    page[1].iov_len = data_size;

    if(!mime) {
        response.content_type = "text/html";
    }
    response.status_code = code;
    response.reason = msg;

    /* send the header */
    page[0].iov_base = calloc(BUFFSIZE, 1);
    page[0].iov_len = BUFFSIZE;
    if(!page[0].iov_base) {
        logging(ERR, "unable to alloc new iovec buffer");
        return -1;
    }
    page[0].iov_len = response_header_write(&response, &page[0]);
    if(!page[0].iov_len) {
        free(page[0].iov_base);
        return -1;
    }

    /* TODO handle err on write */
    ret = SSL_writev(sock, page, 1);
    free(page[0].iov_base);
    return ret-page[0].iov_len;
}

/* reuses a fixed buffer to read from a large file and to write to socket
 * Returns
 *  -1 on error */
static ssize_t send_large_file(
        int fd,
        size_t count,
        SSL *sock,
        int total_blocks,
        struct response_header response) {

    size_t write_size = 0;
    size_t ret;
    struct iovec buff;


    buff.iov_base = malloc(BUFFSIZE);
    buff.iov_len = BUFFSIZE;
    if(!buff.iov_base) return -1;

    if((buff.iov_len = response_header_write(&response, &buff)) == 0) {
        goto failure;
    }
    if(SSL_write(sock, buff.iov_base, buff.iov_len) < 0) {
        goto failure;
    }
    // reset the size of buff for the file
    buff.iov_len = BUFFSIZE;

    size_t cur_count = count;
    for(int i = 0; i < total_blocks -1; i++) {
        if(read(fd, buff.iov_base, MIN(buff.iov_len, cur_count)) < 0) {
            goto failure;
        }
        ret = SSL_write(sock, buff.iov_base, MIN(buff.iov_len, cur_count));
        if(ret <= 0) {
            goto failure;
        }
        write_size += ret;
        cur_count -= buff.iov_len;
    }

    free(buff.iov_base);
    if(count != write_size) {
        logging(DEBUG, "sent and file size don't match %ld != %ld", write_size, count);
    }
    return write_size;

failure:
    free(buff.iov_base);
    return -1;
}

/* Tries to send count char of fd
 * Returns:
 *  the size sent
 *  -1 on fail, check errno */
ssize_t send_file(
        int code,
        char *msg,
        char *mime,
        int fd,
        size_t count,
        SSL *sock) {

    size_t ret;
    struct response_header response;

    response.status_code = code;
    response.reason = msg;
    response.content_type = mime;

    int nb_vecs = count / BUFFSIZE + 1;
    size_t cur_count = count;
    if((count % BUFFSIZE))
        nb_vecs++;

    if(nb_vecs > MAX_BUFF_COUNT_FAST || 1) {
        return send_large_file(
                fd,
                count,
                sock,
                nb_vecs,
                response);
    }

    struct iovec *page = malloc(sizeof(struct iovec) * (nb_vecs));
    if(!page) return -1;


    /* headers */
    page[0].iov_base = malloc(BUFFSIZE);
    if(!page[0].iov_base) {
        free(page);
        return -1;
    }

    page[0].iov_len = BUFFSIZE;
    page[0].iov_len = response_header_write(&response, &page[0]);
    if(!page[0].iov_len) {
        free(page[0].iov_base);
        free(page);
        return -1;
    }
    for(int i = 1; i < nb_vecs; i++) {
        page[i].iov_base = malloc(BUFFSIZE);
        if(!page[i].iov_base) {
            for(int j = 0; j < i; j++) {
                free(page[j].iov_base);
            }
            free(page);
            return -1;
        }
        page[i].iov_len = BUFFSIZE < cur_count ? BUFFSIZE : cur_count;
        cur_count -= BUFFSIZE;
    }

    ssize_t read = readv(fd, &page[1], nb_vecs-1);
    if( read != count) {
        if(read < 0) {
            perror("readv");
        }
        goto failure;
    }
    ret = SSL_writev(sock, page, nb_vecs);
    if(ret < 0) {
        return -1;
    }
    ret -= page[0].iov_len;

    for(int i = 0; i < nb_vecs; i++) {
        free(page[i].iov_base);
    }
    free(page);

    return ret;

failure:
    for(int i = 0; i < nb_vecs; i++) {
        free(page[i].iov_base);
    }
    free(page);
    return -1;
}

/* Tries to send a whole file
 * Returns:
 *  the size sent
 *  -1 on fail, check errno */
ssize_t send_whole_file(
        int code, char *msg,
        char *mime,
        int fd, SSL *sock) {
    /* send a file */
    struct stat stat;
    if(fstat(fd, &stat) == -1) {
        perror("fstat err: ");
        return -1;
    }

    return send_file(code, msg, mime, fd, stat.st_size, sock);
}

int send_404(SSL *sock) {
    return send_str(
            404, "page not found", 0,
            NOT_FOUND_PAGE, NOT_FOUND_PAGE_LEN,
            sock);
}

int send_405(SSL *sock) {
    return send_str(
            405, "this server only supports http GET", 0,
            NOT_FOUND_PAGE, NOT_FOUND_PAGE_LEN,
            sock);
}

int send_500(SSL *sock) {
    return send_str(
            500, "server error", 0,
            SERVER_ERROR_PAGE, SERVER_ERROR_PAGE_LEN,
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
        logging(ERR, "unable to bind %d: `%s`", serv_addr->sin_port, strerror(errno));
        return 1;
    }
    if(listen(*sock_fd, ACCEPT_Q_SIZE) != 0) {
        logging(ERR, "unable to listen on port %d: `%s`", serv_addr->sin_port, strerror(errno));
        close(*sock_fd);
        return 1;
    }
    /* ready to start serving */
    return 0;
}

/* handles a connection
 * Returns
 *  -1 on err */
int handle_conn(SSL *sock) {
    /* depends on basedir, basedir_len and mimes_hmap */
    char buff[BUFFSIZE]={0};
    ssize_t buff_len;
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

    if(SSL_accept(sock) <= 0) {
        ERR_print_errors_fp(stderr);
        logging(ERR, "SSL_accept err");
        return -1;
    }

    memcpy(path_buff, basedir, basedir_len);
    path_buff[basedir_len] = '/';
    int ssl_ret;

    while((buff_len = SSL_read(sock, buff, BUFFSIZE)) < 0) {
        ssl_ret = SSL_get_error(sock, buff_len);
        /*
        if(buff_len < 0) {
            logging(ERR, "SSL_read err");
            SSL_cleanup(sock);
            return -1;
        }
        */

        if(ssl_ret != SSL_ERROR_WANT_READ) {
            logging(ERR, "SSL_read err %d", ssl_ret);
            SSL_cleanup(sock);
            return -1;
        }
    }

    /* nothing to read */
    if(!buff_len) {
        SSL_cleanup(sock);
        logging(WARN, "nothing to read on connection %d, closing", sock);
        return 0;
    }


    /* TODO handle requests larger than BUFFSIZE */

    /* check if the content isn't GET */
    if(strncmp(buff, "GET ", 4)) {
        /* unsuported protocol */
        puts(buff);
        send_405(sock);
        logging(DEBUG, "buff lenght: %ld", buff_len);
        SSL_cleanup(sock);
        return 0;
    }

    if(request_header_parse(&request, buff, BUFFSIZE) < 0) {
        SSL_cleanup(sock);
        return -1;
    }

    request.file++;
    file_len = strlen(request.file);
    /* FIXME temporary workaroud */
    if(file_len == 0) {
        request.file = index;
        file_len = 10;
    }

    memcpy(path_buff + basedir_len + 1, request.file, file_len);
    path_buff[basedir_len + 1 + file_len] = '\0';
    printf("path buff: %s\n", path_buff);

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
            SSL_cleanup(sock);
            return 0;
        }
        /* return a boring old 404 */
        send_404(sock);
        SSL_cleanup(sock);
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
        if(lseek(file, 0, SEEK_SET) <0) {
            perror("lseek");
        }
    }

    /* send the file */
    if(send_whole_file(200, 0, type, file, sock) < 0) {
        send_500(sock);
    }

    close(file);
    SSL_cleanup(sock);
    return 0;
}

/* returns 0 on err */
SSL_CTX* ctx_init(void) {
    const SSL_METHOD *meth;
    SSL_CTX *ctx;

    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    meth = TLS_server_method();
    ctx = SSL_CTX_new(meth);
    if(!ctx) {
        // TODO(louis) add err message
        ERR_print_errors_fp(stderr);
    }
    return ctx;
}

void load_certificates(SSL_CTX *ctx, char *cert_file, char *key_file) {
    if(SSL_CTX_use_certificate_file(
                ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        abort();
    }

    if(SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        abort();
    }

    if(!SSL_CTX_check_private_key(ctx)) {
        logging(ERR, "Private key does not match cert");
        abort();
    }
}

int main(int argc, const char **argv) {
    int port_no;
    struct sockaddr_in serv_addr;
    int serv_fd;

    signal(SIGINT, sigint_halder);
    /* prevent gdb and valgrind to stop execution on SIGPIPE */
#ifndef NDEBUG
    signal(SIGPIPE, sigint_halder);
#endif

    /* initialise openssl  */
    SSL_library_init();

    /* configure ssl */
    SSL_CTX *ctx = ctx_init();
    load_certificates(ctx, "cert0.pem", "cert0.pem");

    /* check parameters */
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

    logging(INFO, "Building MIME type hashmap");
    /* initialise the mime hashmap */
    build_mimes_hmap(&mimes_hmap);

    logging(INFO,"starting server on 0.0.0.0:%d", port_no);
    /* setup socket for listen */
    if(serv_setup(port_no, &serv_fd, &serv_addr)) {
        perror("");
        return -1;
    }

    logging(INFO, "server started");

    /* ###########################Start Serving############################# */
    while(KEEP_RUNNING) {
        int code, nfds = 0;
        fd_set read;
        struct timeval timeval;
        timeval.tv_sec = 5;
        timeval.tv_usec = 0;
        FD_ZERO(&read);
        FD_SET(serv_fd, &read);
        nfds = nfds > serv_fd ? nfds : serv_fd;

        code = select(nfds+1, &read, 0, 0, &timeval);
        if(code == -1) {
            goto cleanup;
        }
        else if(code == 0) {
            continue;
        }
        int new_fd = accept(serv_fd, 0, 0);
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, new_fd);
        if(handle_conn(ssl)) {
            logging(WARN, "An error occurred on connection %d (empty read)", new_fd);
        }
    }
cleanup:
    /* close the socket */
    close(serv_fd);
    SSL_CTX_free(ctx);
    return 0;
}
