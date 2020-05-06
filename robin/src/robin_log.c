#include <stdio.h>

#include "robin_log.h"

static const char log_header_err[]  = "[ERROR]   ";
static const char log_header_warn[] = "[WARNING] ";
static const char log_header_info[] = "[INFO]    ";

void _robin_log_print(robin_log_level_t log_lvl, const char *fmt, ...)
{
    FILE *fp;
    va_list args;
    const char *log_header;

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

    fprintf(fp, "%s", log_header);

    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
}
