#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "config.h"

#define MIN(a,b) (a < b ? a : b)

struct config CONFIG = {
    .bind_addr = "0.0.0.0",
    .http_port = 80,
    .https_port = 443,
    .pem_file = "cert.pem",
    .base_dir = ".",
    .base_dir_len = 2,
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
    line++;
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
        if(*value >= line) return KV_NoValue;
        value_end = line;
    }
    else {
        *value = line;
        line++;
        while(*line && !isspace(*line)) line++;
        value_end = line;
    }
    line++;
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
    int ret_val = 0;
    int line_num = 0;
    char *line = 0;
    ssize_t line_len = 0;
    size_t line_size = 20;

    int split_ret;

    char *key;
    char *value;


    while((line_len = getline(&line, &line_size, f)) != -1) {
        line_num++;
        if(line_len < 0) {
            goto cleanup;
        }
        /* empty line */
        if(line_len <= 1) continue;

        split_ret = key_value_split(line, &key, &value);
        switch(split_ret) {
            case KV_NoKey:
                fprintf(stderr,
                        "line %d: `%s` no key found\n",
                        line_num,
                        line);
                goto cleanup;
            case KV_NoValue:
                fprintf(stderr,
                        "line %d: `%s` no value found\n",
                        line_num,
                        line);
                goto cleanup;
            case KV_NoAssignment:
                fprintf(stderr,
                        "line %d: `%s` no assignment found\n",
                        line_num,
                        line);
                goto cleanup;
            case Kv_UnclosedQuote:
                fprintf(stderr,
                        "line %d: `%s` unclosed quote\n",
                        line_num,
                        line);
                goto cleanup;
            case KV_UnexpectedToken:
                fprintf(
                    stderr,
                    "line %d: `%s` unexpected token after value\n",
                    line_num,
                    line);
                goto cleanup;
            case KV_Comment:
                continue;
        }
        /* must be a line with a key and a value */
        size_t key_len = strlen(key) + 1;
        if(key_len == sizeof("bind_addr")
                && !strncmp("bind_addr", key, key_len)) {
            int value_len = strlen(value);
            CONFIG.bind_addr = malloc(value_len);
            strncpy(CONFIG.bind_addr, value, value_len);
        }
        else if(key_len == sizeof("http_port")
                && !strncmp("http_port", key, key_len)) {
            char *end=0;
            int port = strtol(value, &end, 10);
            if(*end != '\0' || port < 1 || port > 65535) {
                fprintf(stderr,
                        "unable to parse `%s` must be a number between 1 and 65535 inclusively\n",
                        value);
                goto cleanup;
            }
            else {
                CONFIG.http_port = port;
            }
        }
        else if(key_len == sizeof("https_port")
                && !strncmp("https_port", key, key_len)) {
            char *end=0;
            int port = strtol(value, &end, 10);
            if(*end != '\0' || port < 1 || port > 65535) {
                fprintf(stderr,
                        "unable to parse `%s` must be a number between 1 and 65535 inclusively\n",
                        value);
                ret_val = -1;
                goto cleanup;
            }
            else {
                CONFIG.https_port = port;
            }
        }
        else if(key_len == sizeof("pem_file")
                && !strncmp("pem_file", key, key_len)) {
            int value_len = strlen(value);
            CONFIG.pem_file = malloc(value_len);
            strncpy(CONFIG.pem_file, value, value_len);
        }
        else if(key_len == sizeof("base_dir")
                && !strncmp("base_dir", key, key_len)) {
            int value_len = strlen(value);
            CONFIG.base_dir = malloc(value_len);
            CONFIG.base_dir_len = value_len;
            strncpy(CONFIG.base_dir, value, value_len);
        }
        else {
            fprintf(stderr, "unknown key: `%s`\n", key);
        }
    }
cleanup:
    free(line);
    return ret_val;
}

void cleanup_config() {
    free(CONFIG.pem_file);
    free(CONFIG.bind_addr);
}
