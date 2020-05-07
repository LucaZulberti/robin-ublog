/*
 * robin_log.c
 *
 * Log utility for standard log messages across the application.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <stdio.h>
#include <stdlib.h>

#include "robin_log.h"

#define LOG_MSG_BUF_LEN 256

static const char log_header_err[]  = "[ERROR]   ";
static const char log_header_warn[] = "[WARNING] ";
static const char log_header_info[] = "[INFO]    ";

void _robin_log_print(robin_log_level_t log_lvl, const char *fmt, ...)
{
    FILE *fp;
    va_list args;
    const char *log_header;
    char *msg;

    switch(log_lvl) {
        case ROBIN_LOG_ERR:
            fp = stderr;
            log_header = log_header_err;
            break;

        case ROBIN_LOG_WARN:
            fp = stderr;
            log_header = log_header_warn;
            break;

        case ROBIN_LOG_INFO:
            fp = stdout;
            log_header = log_header_info;
            break;
    }

    msg = malloc(LOG_MSG_BUF_LEN * sizeof(char));
    if (!msg)
        msg = "<log error: couldn't allocate memory for log message>";

    va_start(args, fmt);
    vsnprintf(msg, LOG_MSG_BUF_LEN, fmt, args);
    va_end(args);
    fprintf(fp, "%s%s\n", log_header, msg);

    free(msg);
}
