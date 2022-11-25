#include "send.h"

#include <unistd.h>
#include <sys/stat.h>


#define MIN(a,b) (a < b ? a : b)

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
