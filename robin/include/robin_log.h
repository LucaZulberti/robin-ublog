/*
 * robin_log.h
 *
 * Header file containing types, macro and functions definitions for
 * Robin log utility.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_LOG_H
#define ROBIN_LOG_H

#include <stdarg.h>

#define ROBIN_LOG_LEVEL_INFO  2
#define ROBIN_LOG_LEVEL_WARN  1
#define ROBIN_LOG_LEVEL_ERR   0

#define ROBIN_LOG_LEVEL_DEFAULT ROBIN_LOG_LEVEL_INFO

#ifndef ROBIN_LOG_LEVEL
#   define ROBIN_LOG_LEVEL ROBIN_LOG_LEVEL_DEFAULT
#endif

#ifndef ROBIN_LOG_ENABLED
#   define ROBIN_LOG_ENABLED 1
#endif

#if ROBIN_LOG_ENABLED == 1
#   if ROBIN_LOG_LEVEL >= 0
#       define robin_log_err(fmt, ...) \
            _robin_log_print(ROBIN_LOG_ERR, (fmt) __VA_OPT__(,) __VA_ARGS__)
#   else
#       define robin_log_err(fmt, ...)
#   endif
#   if ROBIN_LOG_LEVEL >= 1
#       define robin_log_warn(fmt, ...) \
            _robin_log_print(ROBIN_LOG_WARN, (fmt) __VA_OPT__(,) __VA_ARGS__)
#   else
#       define robin_log_warn(fmt, ...)
#   endif
#   if ROBIN_LOG_LEVEL >= 2
#       define robin_log_info(fmt, ...) \
            _robin_log_print(ROBIN_LOG_INFO, (fmt) __VA_OPT__(,) __VA_ARGS__)
#   else
#       define robin_log_info(fmt, ...)
#   endif
#endif /* ROBIN_LOG_ENABLED */

typedef enum robin_log_level {
    ROBIN_LOG_ERR = 0,
    ROBIN_LOG_WARN = 1,
    ROBIN_LOG_INFO = 2
} robin_log_level_t;

void _robin_log_print(robin_log_level_t log_lvl, const char *fmt, ...);

#endif /* ROBIN_LOG_H */
