#include "conn.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

ssize_t SSL_writev(SSL *ssl, const struct iovec *iov, int iovcnt) {
    ssize_t size = 0;
    ssize_t ret;
    for(int i = 0; i < iovcnt; i++) {
        ret = SSL_write(ssl, iov[i].iov_base, iov[i].iov_len);
        if(ret < 0) {
            return ret;
        }
        size += ret;
    }
    return size;
}

void SSL_cleanup(SSL *ssl) {
    int fd = SSL_get_fd(ssl);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(fd);
}

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

/* flushed the socket's buffer
 * Returns 0 on success and an err code otherwise */
int conn_flush(struct conn *conn) {
    char buf[20];
    int flags;
    int fd = -1;
    int ret;

    switch(conn->type) {
        case CONN_PLAIN:
            fd = conn->data.fd;
        break;
        case CONN_SSL:
            fd = SSL_get_fd(conn->data.ssl);
        break;
    }
    if(fd < 0) return EINVAL;
    /* set non-blocking */
    flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
    do {
        ret = conn_read(conn, buf, 20);
    }
    while(ret > 0);
    if(ret == -1) {
        return errno;
    }
    /* set to blocking again */
    flags &= ~O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);

    return 0;
}
