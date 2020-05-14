/*
 * robin_log.h
 *
 * Header file containing types, macros and public interface for
 * Robin log utility.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_LOG_H
#define ROBIN_LOG_H

#define ROBIN_LOG_LEVEL_DEBUG 3
#define ROBIN_LOG_LEVEL_INFO  2
#define ROBIN_LOG_LEVEL_WARN  1
#define ROBIN_LOG_LEVEL_ERR   0

#define ROBIN_LOG_LEVEL_DEFAULT ROBIN_LOG_LEVEL_INFO

#ifndef ROBIN_LOG_LEVEL
#   if defined DEBUG && DEBUG == 1
#       define ROBIN_LOG_LEVEL ROBIN_LOG_LEVEL_DEBUG
#   else
#       define ROBIN_LOG_LEVEL ROBIN_LOG_LEVEL_DEFAULT
#   endif
#endif

#ifndef ROBIN_LOG_ENABLED
#   define ROBIN_LOG_ENABLED 1
#endif

#if ROBIN_LOG_ENABLED == 1
#   if ROBIN_LOG_LEVEL >= ROBIN_LOG_LEVEL_ERR
#       define robin_log_err(id, fmt, args...) \
            _robin_log_print(ROBIN_LOG_ERR, id, fmt " (%s: %d)\n", ## args, \
                             __FILE__, __LINE__)
#   else
#       define robin_log_err(id, fmt, ...)
#   endif
#   if ROBIN_LOG_LEVEL >= ROBIN_LOG_LEVEL_WARN
#       define robin_log_warn(id, fmt, args...) \
            _robin_log_print(ROBIN_LOG_WARN, id, fmt "\n", ## args)
#   else
#       define robin_log_warn(id, fmt, ...)
#   endif
#   if ROBIN_LOG_LEVEL >= ROBIN_LOG_LEVEL_INFO
#       define robin_log_info(id, fmt, args...) \
            _robin_log_print(ROBIN_LOG_INFO, id, fmt "\n", ## args)
#   else
#       define robin_log_info(id, fmt, ...)
#   endif
#   if ROBIN_LOG_LEVEL >= ROBIN_LOG_LEVEL_DEBUG
#       define robin_log_dbg(id, fmt, args...) \
            _robin_log_print(ROBIN_LOG_DEBUG, id, fmt " (%s: %d)\n", ## args, \
                             __FILE__, __LINE__)
#   else
#       define robin_log_dbg(id, fmt, ...)
#   endif
#endif /* ROBIN_LOG_ENABLED */

typedef enum robin_log_id {
    ROBIN_LOG_ID_LOG = 0,
    ROBIN_LOG_ID_MAIN,
    ROBIN_LOG_ID_POOL,
    ROBIN_LOG_ID_SOCKET,
    ROBIN_LOG_ID_USER,
    ROBIN_LOG_ID_RT_BASE = 1000
} robin_log_id_t;

typedef enum robin_log_level {
    ROBIN_LOG_ERR = 0,
    ROBIN_LOG_WARN,
    ROBIN_LOG_INFO,
    ROBIN_LOG_DEBUG
} robin_log_level_t;

/**
 * @brief Actually the function that perform logging.
 *
 * @param log_lvl log level
 * @param id      id for log identification
 * @param fmt     printf-style format argument
 * @param ...     optional arguments for fmt expansion
 */
void _robin_log_print(robin_log_level_t log_lvl, robin_log_id_t id, const char *fmt, ...);

#endif /* ROBIN_LOG_H */
