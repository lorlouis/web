#ifndef CONFIG_H
#define CONFIG_H 1
#include <stdio.h>

struct config {
    char *bind_addr;
    int http_port;
    int https_port;
    char *pem_file;
    char *base_dir;
    size_t base_dir_len;
};

extern struct config CONFIG;

enum KV_SPLIT_ERR {
    KV_NoKey = -1,
    KV_NoValue = -2,
    KV_NoAssignment = -3,
    Kv_UnclosedQuote = -4,
    KV_UnexpectedToken = -5,
    KV_Comment = -99,
};

/* saves the config in memory to `f`
 * Returns: < 0 on error, 0 otherwise*/
int save_config(FILE *f);


/* loads a config from `f`
 * Returns: < 0 on error, 0 otherwise */
int load_config(FILE *f);

void cleanup_config();

#endif
