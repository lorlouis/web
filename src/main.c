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
#include "response_header.h"

#include "send.h"

#include <openssl/err.h>
#include <magic.h>

#include "conn.h"
#include "config.h"

#define ACCEPT_Q_SIZE 256

static volatile bool KEEP_RUNNING = true;

static const uint8_t SSL_HELLO_BYTES[][3] = {
    {0x16, 0x03, 0x01}, // 3.1
    {0x16, 0x03, 0x02}, // 1.1
    {0x16, 0x03, 0x03}, // 1.2
    {0x16, 0x02, 0x00}, // 2.0
    {0x16, 0x03, 0x00}, // 3.0
};

static const size_t SSL_HELLO_VARIANTS = sizeof(SSL_HELLO_BYTES) / sizeof(uint8_t[3]);

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

            // TODO(louis) use the values in the config
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
        logging(ERR, "unable to load config: %s", get_config_err());
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
        fd_set r;
        struct timeval timeval;
        timeval.tv_sec = 5;
        timeval.tv_usec = 0;
        FD_ZERO(&r);
        FD_SET(serv_fd, &r);
        nfds = nfds > serv_fd ? nfds : serv_fd;

        code = select(nfds+1, &r, 0, 0, &timeval);
        if(code == -1) {
            goto cleanup;
        }
        else if(code == 0) {
            continue;
        }
        int new_fd = accept(serv_fd, 0, 0);
        struct conn conn = {0};

        uint8_t first_tree_bytes[3] = {0};
        int ret = recv(new_fd, first_tree_bytes, 3, MSG_PEEK);
        assert(ret != -1);

        // Detect ssl headers and default to plain text,
        // this is very cursed and should never be done.
        _Bool is_ssl = false;
        for(size_t i = 0; i < SSL_HELLO_VARIANTS; i++) {
            if(!memcmp(SSL_HELLO_BYTES[i], first_tree_bytes, 3)) {
                SSL *ssl = SSL_new(ctx);
                SSL_set_fd(ssl, new_fd);
                conn_new_ssl(ssl, &conn);
                is_ssl = true;
                break;
            }
        }
        if(!is_ssl) {
            conn_new_fd(new_fd, &conn);
        }
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
