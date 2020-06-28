/*
 * utility.c
 *
 * Utility functions
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <stdlib.h>
#include <string.h>

#include "robin.h"
#include "lib/utility.h"


/*
 * Log shortcuts
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_UTILITY, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_UTILITY, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_UTILITY, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_UTILITY, fmt, ## args)


/*
 * Exported functions
 */

int argv_parse(char *src, int *argc, char ***argv)
{
    char *start_arg, *end_arg, **av;
    int last = 0, ac;

    av = *argv;
    ac = 0;

    start_arg = src;
    do {
        if (*start_arg == ' ') {
            /* discard continuos whitespaces */
            while (*start_arg == ' ')
                start_arg++;
        }

        if (*start_arg == '\0') {
            /* no more arguments */
            return 0;
        } else if (*start_arg == '"') {
            /* if next arg starts with double quotes, search for the closing
            * double quotes to store the whole string as one argument
            */
            end_arg = strchr(++start_arg, '"');
            if (!end_arg)
                return 0;
        } else
            end_arg = strchr(start_arg, ' ');

        if (end_arg)
            *end_arg = '\0';
        else
            last = 1;

        av = realloc(av, (ac + 1) * sizeof(char *));
        if (!av) {
            err("realloc: %s", strerror(errno));
            return -1;
        }


        dbg("argv_parse: arg #%d: %s", ac, start_arg);
        av[ac++] = start_arg;  /* store new arg */

        *argv = av;
        *argc = ac;

        start_arg = end_arg + 1;
    } while (!last);

    return 0;
}
