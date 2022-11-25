#ifndef SEND_H
#define SEND_H 1

#include "response_header.h"
#include "logging.h"
#include "conn.h"

#include "default_pages.h"


#define BUFFSIZE 4096
#define MAX_BUFF_COUNT_FAST 128


/* Tries to send data_size from data into sock
 * Returns
 *  the size sent
 *  -1 on fail, check errno */
ssize_t send_str(
        struct response_header *response,
        const char *data,
        size_t data_size,
        struct conn *sock);

/* Tries to send a whole file
 * Returns:
 *  the size sent
 *  -1 on fail, check errno */
ssize_t send_whole_file(
        int code, char *msg,
        const char *mime,
        int fd,
        struct conn *sock);

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
        struct conn *sock);


int send_404(struct conn *sock);

int send_405(struct conn *sock);

int send_500(struct conn *sock);

int send_308(struct conn *sock, char *location);

int send_426(struct conn *sock, const char *protocol);

#endif
