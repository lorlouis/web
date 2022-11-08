#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define MIN(a,b) (a < b ? a : b)

struct {
    char *bind_addr;
    int http_port;
    int https_port;
    char *pem_file;
} CONFIG = {
    .bind_addr = "0.0.0.0",
    .http_port = 80,
    .https_port = 443,
    .pem_file = "cert.pem",
};

/* saves the config in memory to `f`
 * Returns: < 0 on error, 0 otherwise*/
int save_config(FILE *f) {
    int ret;
    ret = fprintf(f, "bind_addr = \"%s\"\n", CONFIG.bind_addr);
    if(ret < 0) return ret;
    ret = fprintf(f, "http_port = %d\n", CONFIG.http_port);
    if(ret < 0) return ret;
    ret = fprintf(f, "https_port = %d\n", CONFIG.https_port);
    if(ret < 0) return ret;
    ret = fprintf(f, "pem_file = \"%s\"\n", CONFIG.pem_file);
    if(ret < 0) return ret;
    return 0;
}

enum KV_SPLIT_ERR {
    KV_NoKey = -1,
    KV_NoValue = -2,
    KV_NoAssignment = -3,
    Kv_UnclosedQuote = -4,
    KV_UnexpectedToken = -5,
    KV_Comment = -99,
};

/* Extracts the key and the value out of a line formatted like
 * <key> = <value>\0 -> <key>\0= <value>\0
 * Returns: < 0 on err, 0 otherwise */
static int key_value_split(char *line, char **key, char **value) {
    char *key_end;
    char *value_end;

    int is_quoted = 0;
    /* skip white spaces */
    while(*line && isspace(*line)) line++;
    /* no key */
    if(!*line) return KV_NoKey;
    /* a comment */
    if(*line == '#') return KV_Comment;
    /* set the start of the key */
    *key = line;
    /* find end of key */
    while(*line && !isspace(*line)) line++;
    /* no value */
    if(!*line) return KV_NoValue;
    key_end = line;
    line++;
    /* find = */
    while(*line && *line != '=') line++;
    /* no assignment */
    if(!*line) return KV_NoAssignment;
    /* skip white spaces */
    while(*line && isspace(*line)) line++;
    /* no value */
    if(!*line) return KV_NoValue;
    /* find end of value */
    if(*line == '"') {
        line++;
        *value = line;
        while(*line && *line != '"') line++;
        /* unclosed quote */
        if(!*line) return Kv_UnclosedQuote;
        /* no value */
        if(*value >= line-1) return KV_NoValue;
        value_end = line-1;
    }
    else {
        *value = line;
        while(*line && !isspace(*line)) line++;
        /* end of the line no need to check further */
        if(!*line) return 0;
        value_end = line;
    }

    /* check for unexpected values */
    while(*line && isspace(*line)) line++;
    /* unexpected value */
    if(*line) return KV_UnexpectedToken;
    *key_end = '\0';
    *value_end = '\0';
    return 0;
}

/* loads a config from `f`
 * Returns: < 0 on error, 0 otherwise */
int load_config(FILE *f) {
    int line_num = 0;
    char *line = 0;
    ssize_t line_len = 0;
    size_t line_size = 0;

    int split_ret;

    char *key;
    char *value;

    while((line_len = getline(&line, &line_size, f))) {
        line_num++;
        if(line_len < 0) goto cleanup;
        /* empty line */
        if(line_len <= 1) continue;

        split_ret = key_value_split(line, &key, &value);
        switch(split_ret) {
            case KV_NoKey:
                fprintf(stderr,
                        "line %d: `%s` no key found\n",
                        line_num,
                        line);
                break;
            case KV_NoValue:
                fprintf(stderr,
                        "line %d: `%s` no value found\n",
                        line_num,
                        line);
                break;
            case KV_NoAssignment:
                fprintf(stderr,
                        "line %d: `%s` no assignment found\n",
                        line_num,
                        line);
                break;
            case Kv_UnclosedQuote:
                fprintf(stderr,
                        "line %d: `%s` unclosed quote\n",
                        line_num,
                        line);
                break;
            case KV_UnexpectedToken:
                fprintf(
                    stderr,
                    "line %d: `%s` unexpected token after value\n",
                    line_num,
                    line);
                break;
            case KV_Comment:
                continue;
        }
        /* must be a line with a key and a value */
        size_t key_len = strlen(key);
        if(key_len == sizeof("bind_addr")
                && !strncmp("bind_addr", key, key_len)) {
            printf("bind addr");
        }

    }
cleanup:
    free(line);
    return line_len;
}
