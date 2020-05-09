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

void _robin_log_print(robin_log_level_t log_lvl, robin_log_id_t id, const char *fmt, ...)
{
    FILE *fp;
    va_list args, test_args;
    const char *log_hdr;
    char *msg, *id_str;
    int msg_len;

    switch(log_lvl) {
        case ROBIN_LOG_ERR:
            fp = stderr;
            log_hdr = "[ERROR]  ";
            break;

        case ROBIN_LOG_WARN:
            fp = stderr;
            log_hdr = "[WARNING]";
            break;

        case ROBIN_LOG_INFO:
            fp = stdout;
            log_hdr = "[INFO]   ";
            break;
    }

    va_start(args, fmt);
    va_copy(test_args, args);

    msg_len = vsnprintf(NULL, 0, fmt, test_args);
    va_end(test_args);

    msg = malloc(msg_len * sizeof(char));
    if (!msg)
        msg = "<couldn't allocate memory for log message>";

    vsnprintf(msg, msg_len + 1, fmt, args);
    va_end(args);

    if (id < ROBIN_LOG_ID_RT_BASE) {
        switch (id) {
            case ROBIN_LOG_ID_LOG:
                id_str = "logger";
                break;

            case ROBIN_LOG_ID_MAIN:
                id_str = "main";
                break;

            case ROBIN_LOG_ID_POOL:
                id_str = "rt_pool";
                break;

            case ROBIN_LOG_ID_SOCKET:
                id_str = "socket";
                break;

            default:
                id_str = "???";
                break;
        }
        fprintf(fp, "%s %s: %s", log_hdr, id_str, msg);
    } else {
        fprintf(fp, "%s rt#%d: %s", log_hdr, id - ROBIN_LOG_ID_RT_BASE, msg);
    }

    free(msg);
}
