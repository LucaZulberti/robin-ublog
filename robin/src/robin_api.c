/*
 * robin_api.c
 *
 * Robin API, used as static library from clients.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include "robin.h"


/*
 * Log shortcuts
 */

#define err(fmt, args...)  robin_log_err(conn->log_id, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(conn->log_id, fmt, ## args)
#define info(fmt, args...) robin_log_info(conn->log_id, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(conn->log_id, fmt, ## args)


/*
 * Local data
 */


/*
 * Exported functions
 */

int robin_api_init(int fd)
{

}

int robin_api_register(const char *email, const char *password)
{

}

int robin_api_login(const char *email, const char *password)
{

}

int robin_api_logout(void)
{

}
