#include "logging.h"

#define XSTR(s) STR(s)
#define STR(s) #s

const char *LOG_LEVEL_STR[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERR",
};

/* log to stderr (includes newline) */
void logging(enum log_level level, char *fmt, ...) {
    char time_buff[80] = {0};
    FILE *const out_fd = stderr;
    const size_t time_buff_size = sizeof(time_buff) / sizeof(char) -1;
    va_list args;
    char *ts;
    time_t cur_time;
    struct tm local_time;

    cur_time = time(0);
    if(cur_time == -1) {
        ts = "TIME_ERR";
        goto print;
    }
    if(!localtime_r(&cur_time, &local_time)) {
        ts = "TIME_ERR";
        goto print;
    }
    if(!strftime(time_buff, time_buff_size, "%d/%b/%Y:%H:%M:%S %z", &local_time)) {
        ts = "TIME_ERR";
        goto print;
    }
    // set ts to the real buffer
    ts = time_buff;
print:

    fprintf(out_fd, "[%s] [%s] \"", LOG_LEVEL_STR[level], ts);
    va_start(args, fmt);
    vfprintf(out_fd, fmt, args);
    va_end(args);
    /* insert newline */
    puts("\"");
}
