#include "ssl_ex.h"
#include "conn.h"
#include <unistd.h>

void conn_cleanup(struct conn *conn) {
    switch(conn->type) {
        case CONN_PLAIN:
            close(conn->data.fd);
            break;
        case CONN_SSL:
            SSL_cleanup(conn->data.ssl);
            break;
    }
}

ssize_t conn_read(struct conn *conn, void *buf, size_t size) {
    switch(conn->type) {
        case CONN_PLAIN:
            return read(conn->data.fd, buf, size);
        case CONN_SSL:
            return SSL_read(conn->data.ssl, buf, (int)size);
    }
    return -1;
}

ssize_t conn_write(struct conn *conn, const void *buf, size_t size) {
    switch(conn->type) {
        case CONN_PLAIN:
            return write(conn->data.fd, buf, size);
        case CONN_SSL:
            return SSL_write(conn->data.ssl, buf, (int)size);
    }
    return -1;
}

ssize_t conn_writev(struct conn *conn, const struct iovec *iov, size_t nbv) {
    switch(conn->type) {
        case CONN_PLAIN:
            return writev(conn->data.fd, iov, nbv);
        case CONN_SSL:
            return SSL_writev(conn->data.ssl, iov, nbv);
    }
    return -1;
}

int conn_new_fd(int fd, struct conn *conn) {
    conn->type = CONN_PLAIN;
    conn->data.fd = fd;
    return 0;
}

int conn_ssl_to_conn_fd(struct conn *conn) {
    if(conn->type == CONN_SSL) {
        int fd;
        conn->type = CONN_PLAIN;
        fd = SSL_get_fd(conn->data.ssl);
        SSL_free(conn->data.ssl);
        conn->data.fd = fd;
        return fd;
    }
    return -1;
}

int conn_new_ssl(SSL *ssl, struct conn *conn) {
    conn->type = CONN_SSL;
    conn->data.ssl = ssl;
    return 0;
}

int conn_init(struct conn *conn) {
    switch(conn->type) {
        case CONN_PLAIN:
            return 1;
        case CONN_SSL:
            return SSL_accept(conn->data.ssl);
    }
    return 0;
}
