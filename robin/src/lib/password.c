#include <stdlib.h>

#include <crypt.h>

#include "robin.h"
#include "robin_log.h"
#include "lib/password.h"


/*
 * Log shortcut
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_PASSWORD, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_PASSWORD, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_PASSWORD, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_PASSWORD, fmt, ## args)


/*
 * Local functions
 */

static char char_for_salt(void)
{
    int r, t1, t2, t3, tot;

    t1 = '9' - '.' + 1;
    t2 = 'z' - 'a' + 1;
    t3 = 'Z' - 'A' + 1;
    tot = t1 + t2 + t3;

    r = rand() % tot;

    return (r < t1) ? r + '.' :
           (r < t1 + t2) ? r - t1 + 'a' :
           r - t1 - t2 + 'A';
}


/*
 * Exported functions
 */

int password_hash(char *psw_hashed, const char *psw, const char *salt)
{
    struct crypt_data *data;
    char salt_to_use[3], *tmp;

    if (salt) {
        salt_to_use[0] = salt[0];
        salt_to_use[1] = salt[1];
    } else {
        /* generate salt */
        salt_to_use[0] = char_for_salt();
        salt_to_use[1] = char_for_salt();
    }
    salt_to_use[2] = '\0';

    data = malloc(sizeof(struct crypt_data));
    if (!data) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    tmp = crypt_r(psw, salt_to_use, data);
    if (!tmp || tmp[0] == '*') {
        err("crypt_r: %s", strerror(errno));
        return -1;
    }

    strcpy(psw_hashed, tmp);

    free(data);

    return 0;
}
