#ifndef LOGGING_H
#define LOGGING_H 1

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

enum log_level {
    DEBUG = 0,
    INFO,
    WARN,
    ERR,
};

/* calls strerror on errno and logs it*/
void logging_errno(enum log_level level, char *prefix);

/* log to stderr (includes newline) */
void logging(enum log_level level, char *fmt, ...);

#endif
