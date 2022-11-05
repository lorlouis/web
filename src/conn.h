#ifndef CONN_H
#define CONN_H 1

#include "ssl_ex.h"
#include <unistd.h>

enum conn_type {
    CONN_PLAIN,
    CONN_SSL,
};

struct conn {
    enum conn_type type;
    union {
        SSL *ssl;
        int fd;
    } data;
};

void conn_cleanup(struct conn *conn);

ssize_t conn_read(struct conn *conn, void *buf, size_t size);

ssize_t conn_write(struct conn *conn, const void *buf, size_t size);

int conn_new_fd(int fd, struct conn *conn);

int conn_new_ssl(SSL *ssl, struct conn *conn);

int conn_ssl_to_conn_fd(struct conn *conn);

/* performs handshake if the connection is ssl
 * Returns
 * 1 on success
 * <=0 on failure */
int conn_init(struct conn *conn);

ssize_t conn_writev(struct conn *conn, const struct iovec *iov, size_t nbv);

/* flushed the socket's buffer
 * Returns 0 on success and an err code otherwise */
int conn_flush(struct conn *conn);

#endif
