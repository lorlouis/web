#include <sys/socket.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <stdbool.h>
#include "headers.h"
#include "logging.h"
#include "default_pages.h"
#include "response_header.h"

#include <openssl/err.h>
#include <magic.h>

#include "conn.h"
#include "config.h"

#define BUFFSIZE 4096
#define MAX_BUFF_COUNT_FAST 128
#define ACCEPT_Q_SIZE 256

static volatile bool KEEP_RUNNING = true;

#define MIN(a,b) (a < b ? a : b)

magic_t magic;

int mime_init(void) {
    magic = magic_open(MAGIC_MIME_TYPE);
    magic_load(magic, 0);
    // on arch the db is already compiled
    //magic_compile(magic, 0);
    return 0;
}

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


/* Tries to send data_size from data into sock
 * Returns
 *  the size sent
 *  -1 on fail, check errno */
ssize_t send_str(
        struct response_header *response,
        const char *data,
        size_t data_size,
        struct conn *sock) {
    struct iovec page[2] = {0};
    ssize_t ret;

    page[1].iov_base = (void*)data;
    page[1].iov_len = data_size;

    /* send the header */
    page[0].iov_base = calloc(BUFFSIZE, 1);
    page[0].iov_len = BUFFSIZE;
    if(!page[0].iov_base) {
        logging(ERR, "unable to alloc new iovec buffer");
        return -1;
    }
    ret = response_header_write(response, &page[0]);
    page[0].iov_len = ret;
    if(ret <= 0) {
        free(page[0].iov_base);
        logging(ERR, "unable to write response header into iovec");
        return -1;
    }

    /* TODO handle err on write */
    ret = conn_writev(sock, page, 2);
    if(ret <= 0) {
        logging(ERR, "unable to write str response into iovec");
        free(page[0].iov_base);
        return -1;
    }
    free(page[0].iov_base);
    return ret-page[0].iov_len;
}

/* reuses a fixed buffer to read from a large file and to write to socket
 * Returns
 *  -1 on error */
static ssize_t send_large_file(
        int fd,
        size_t count,
        struct conn *sock,
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
    if(conn_write(sock, buff.iov_base, buff.iov_len) < 0) {
        goto failure;
    }
    // reset the size of buff for the file
    buff.iov_len = BUFFSIZE;

    size_t cur_count = count;
    for(int i = 0; i < total_blocks -1; i++) {
        if(read(fd, buff.iov_base, MIN(buff.iov_len, cur_count)) < 0) {
            goto failure;
        }
        ret = conn_write(sock, buff.iov_base, MIN(buff.iov_len, cur_count));
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
        const char *mime,
        int fd,
        size_t count,
        struct conn *sock) {

    size_t ret;
    struct response_header response = {0};

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
            /* free iovecs up to this point */
            for(int j = 0; j < i; j++) {
                free(page[j].iov_base);
            }
            free(page);
            /* no need to jump to failure */
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
    ret = conn_writev(sock, page, nb_vecs);
    if(ret < 0) {
        goto failure;
    }
    ret -= page[0].iov_len;

    /* cleanup */
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
        const char *mime,
        int fd,
        struct conn *sock) {
    /* send a file */
    struct stat stat;
    if(fstat(fd, &stat) == -1) {
        logging_errno(ERR, "fstat");
        return -1;
    }

    return send_file(code, msg, mime, fd, stat.st_size, sock);
}

int send_404(struct conn *sock) {
    struct response_header response = {0};
    response_header_init(&response, 404, "page not found", 0);

    return send_str(
            &response,
            NOT_FOUND_PAGE,
            NOT_FOUND_PAGE_LEN,
            sock);
}

int send_405(struct conn *sock) {
    struct response_header response = {0};
    response_header_init(
            &response,
            405,
            "this server only supports http GET",
            0);

    return send_str(
            &response,
            NOT_FOUND_PAGE,
            NOT_FOUND_PAGE_LEN,
            sock);
}

int send_500(struct conn *sock) {
    struct response_header response = {0};
    response_header_init(
            &response,
            500,
            "server error",
            0);

    return send_str(
            &response,
            SERVER_ERROR_PAGE, SERVER_ERROR_PAGE_LEN,
            sock);
}

int send_308(struct conn *sock, char *location) {
    struct response_header response = {0};
    response_header_init(
            &response,
            308,
            "upgrade",
            0);
    struct key_value kv = {0};
    kv.key = "Location";
    kv.value = location;

    kv_vec_push(
        &response.key_values,
        kv);


    return send_str(
            &response,
            "",
            0,
            sock);
}

int send_426(struct conn *sock, const char *protocol) {
    struct response_header response = {0};
    response_header_init(
            &response,
            426,
            "upgrade",
            0);
    struct key_value kv = {0};
    kv.key = "Connection";
    kv.value = "Upgrade";

    kv_vec_push(
        &response.key_values,
        kv);

    kv.key = "Upgrade";
    kv.value = (char*)protocol;

    kv_vec_push(
        &response.key_values,
        kv);

    return send_str(
            &response,
            UPGRADE_REQUIRED_PAGE,
            UPGRADE_REQUIRED_PAGE_LEN,
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

/* handles a connection */
void handle_conn(struct conn sock) {
    /* depends on basedir, basedir_len and mimes_hmap */
    char buff[BUFFSIZE]={0};
    ssize_t buff_len;
    char path_buff[BUFFSIZE]={0};
    int file=-1;

    struct request_header request = {0};
    char index[] = "index.html";

    size_t file_len = 0;

    const char *type = 0;

    const char html_begin[] = "<!DOCTYPE html>";
    const size_t html_begin_size = sizeof(html_begin);
    static_assert(sizeof(html_begin) == 16, "weird compiler");

    if(conn_init(&sock) <= 0) {
        if(sock.type == CONN_SSL) {
            logging(INFO, "Invalid SSL or plain text connection");
            conn_ssl_to_conn_fd(&sock);

            conn_flush(&sock);

            send_308(&sock, "https://localhost:9092");

            goto cleanup;
        }
    }

    memcpy(path_buff, CONFIG.base_dir, CONFIG.base_dir_len);
    path_buff[CONFIG.base_dir_len] = '/';

    /* nothing to read */
    if((buff_len = conn_read(&sock, buff, BUFFSIZE)) < 0) {
        conn_cleanup(&sock);
        logging(WARN, "nothing to read on connection %d, closing", sock);
        goto cleanup;
    }

    /* check if the content isn't GET */
    if(strncmp(buff, "GET ", 4)) {
        /* unsuported protocol */
        printf("buff: %s", buff);
        send_405(&sock);
        logging(DEBUG, "buff lenght: %ld", buff_len);
        goto cleanup;
    }

    if(request_header_parse(&request, buff, BUFFSIZE) < 0) {
        goto cleanup;
    }

    request.file++;
    file_len = strlen(request.file);
    /* FIXME temporary workaroud */
    if(file_len == 0) {
        request.file = index;
        file_len = 10;
    }

    memcpy(path_buff + CONFIG.base_dir_len + 1, request.file, file_len);
    path_buff[CONFIG.base_dir_len + 1 + file_len] = '\0';
    printf("path buff: %s\n", path_buff);

    /* open the file */
    file = open(path_buff, O_RDONLY);

    /* file not found */
    if(file == -1) {
        /* check for the very important teapot */
        if(!strcmp(request.file, "teapot")) {
            struct response_header response = {0};
            response_header_init(
                    &response,
                    418,
                    "I'm a tea pot",
                    0);

            send_str(&response,
                     I_AM_A_TEAPOT,
                     I_AM_A_TEAPOT_LEN,
                     &sock);
        }
        /* return a boring old 404 */
        else {
            send_404(&sock);
        }
        goto cleanup;
    }
    /* ##### At this point a file is found ##### */
    int path_len = strlen(path_buff);
    /* by default the linux mimetype database does not include css for some
     * reason */
    if(path_len + 4 < BUFFSIZE && !strncmp(path_buff+path_len-4, ".css", 4)) {
        type = "text/css";
    }

    /* set MIME info */
    if(!type) {
        type = magic_file(magic, path_buff);
    }
    if(!type) {
        type = "application/octet-stream";
        /* support for untagged html pages */
        if(read(file, buff, html_begin_size-1) == html_begin_size-1
           && !strncmp(buff, html_begin, html_begin_size-1)) {
            type = "text/html";
        }
        if(lseek(file, 0, SEEK_SET) <0) {
            logging_errno(ERR, "lseek");
            goto cleanup;
        }
    }

    /* send the file */
    if(send_whole_file(200, 0, type, file, &sock) < 0) {
        send_500(&sock);
        goto cleanup;
    }

cleanup:
    conn_cleanup(&sock);
    return;
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
    struct sockaddr_in serv_addr;
    int serv_fd;

    signal(SIGINT, sigint_halder);
    /* prevent gdb and valgrind to stop execution on SIGPIPE */
#ifndef NDEBUG
    signal(SIGPIPE, sigint_halder);
#endif

    if(argc == 1) {
        fprintf(stderr, "Usage: %s <config path>\n", argv[0]);
        return -1;
    }
    FILE *config_file = fopen(argv[1], "r");

    if(config_file <= 0) {
        perror("");
        return -1;
    }
    if(load_config(config_file)) {
        fprintf(stderr, "err loading config\n");
        return -1;
    }

    /* initialise openssl  */
    SSL_library_init();

    /* configure ssl */
    SSL_CTX *ctx = ctx_init();
    load_certificates(ctx, CONFIG.pem_file, CONFIG.pem_file);

    logging(INFO, "Initiating MIME DB");
    /* initialise the mime hashmap */
    mime_init();

    logging(INFO,"starting server on 0.0.0.0:%d", CONFIG.https_port);
    /* setup socket for listen */
    if(serv_setup(CONFIG.https_port, &serv_fd, &serv_addr)) {
        perror("err setup");
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
        struct conn conn = {0};
        conn_new_ssl(ssl, &conn);
        //conn_new_fd(new_fd, &conn);
        /* TODO(louis) multi thread */
        handle_conn(conn);
    }
cleanup:
    /* close the socket */
    close(serv_fd);
    SSL_CTX_free(ctx);
    magic_close(magic);
    cleanup_config();
    return 0;
}
