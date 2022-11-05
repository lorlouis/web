#ifndef SSL_EX_H
#define SSL_EX_H 1

#include <stdio.h>

#include <sys/uio.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/* like writev but on an ssl rather than a raw fd */
ssize_t SSL_writev(SSL *ssl, const struct iovec *iov, int iovcnt);

/* Closes the SSL connection
 * and closes `ssl`'s fd
 * frees `ssl` */
void SSL_cleanup(SSL *ssl);

#endif
