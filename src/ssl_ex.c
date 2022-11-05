#include "ssl_ex.h"
#include <unistd.h>

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
