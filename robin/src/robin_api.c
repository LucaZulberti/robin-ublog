/*
 * robin_api.c
 *
 * Robin API, used as static library from clients.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "robin.h"
#include "lib/socket.h"

/*
 * Log shortcuts
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_API, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_API, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_API, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_API, fmt, ## args)


/*
 * Local types and macros
 */

#define ROBIN_REPLY_LINE_MAX_LEN 300

static int _ra_send(const char *fmt, ...);
#define ra_send(fmt, args...) _ra_send(fmt "\n", ## args)


/*
 * Local data
 */

static int client_fd;
static char *msg_buf = NULL, *reply_buf = NULL;


/*
 * Local functions
 */

static int _ra_send(const char *fmt, ...)
{
    va_list args;
    int msg_len;

    va_start(args, fmt);
    msg_len = vsnprintf(NULL, 0, fmt, args) + 1;
    va_end(args);

    dbg("ra_send: msg_len=%d", msg_len);

    msg_buf = realloc(msg_buf, msg_len * sizeof(char));
    if (!msg_buf) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    va_start(args, fmt);
    if (vsnprintf(msg_buf, msg_len, fmt, args) < 0) {
        err("vsnprintf: %s", strerror(errno));
        return -1;
    }
    va_end(args);

    dbg("ra_send: msg_buf=%.*s", msg_len - 2, msg_buf);

    /* do not send '\0' in msg */
    if (socket_sendn(client_fd, msg_buf, msg_len - 1) < 0) {
        err("socket_sendn: failed to send data to socket");
        return -1;
    }

    return 0;
}

static int ra_wait_reply(char ***lines, int *nline)
{
    char vbuf[ROBIN_REPLY_LINE_MAX_LEN], **l;
    int nbuf;
    int reply_ret;
    size_t reply_len = 0;

    nbuf = socket_recvline(&reply_buf, &reply_len, client_fd,
                          vbuf, ROBIN_REPLY_LINE_MAX_LEN);
    if (nbuf < 0) {
        err("wait_reply: failed to receive a line from the server");
        return -1;
    }

    /* first line contains the number of lines in reply (except the first),
     * or error code if < 0
     */
    reply_ret = strtol(vbuf, NULL, 10);
    if (reply_ret < 0) {
        l = calloc(1, sizeof(char *));
    } else {
        l = calloc(reply_ret + 1, sizeof(char *));
    }

    if (!l) {
        err("calloc: %s", strerror(errno));
        return -1;
    }

    dbg("wait_reply: reply_ret=%d", reply_ret);

    /* always store first line (it is not counted in nline) */
    l[0] = malloc(nbuf * sizeof(char));
    if (!l[0]) {
        err("malloc: %s", strerror(errno));
        goto free_lines_error;
    }

    memcpy(l[0], vbuf, nbuf - 1);   /* do not copy '\n' */
    l[0][nbuf - 1] = '\0';

    if (reply_ret > 0) {
        for (int i = 0; i < reply_ret; i++) {
            nbuf = socket_recvline(&reply_buf, &reply_len, client_fd,
                                   vbuf, ROBIN_REPLY_LINE_MAX_LEN);
            if (nbuf < 0) {
                err("wait_reply: failed to receive a line from the server");
                goto free_lines_error;
            }

            l[i + 1] = malloc(nbuf * sizeof(char));
            if (!l[i + 1]) {
                err("malloc: %s", strerror(errno));
                goto free_lines_error;
            }

            memcpy(l[i + 1], vbuf, nbuf - 1);  /* do not copy '\n' */
            l[i + 1][nbuf - 1] = '\0';
        }
    }

    *lines = l;
    *nline = reply_ret;

    return 0;

free_lines_error:
    if (reply_ret >= 0)
        for (int i = 0; i < reply_ret + 1; i++)
            if (l[i])
                free(l[i]);
            else
                break;
    else
        free(l[0]);

    free(l);

    return -1;
}


/*
 * Exported functions
 */

int robin_api_init(int fd)
{
    client_fd = fd;

    return 0;
}

void robin_api_free(void)
{
    if (msg_buf) {
        dbg("free: msg_buf=%p", msg_buf);
        free(msg_buf);
    }

    if (reply_buf) {
        dbg("free: reply_buf=%p", reply_buf);
        free(reply_buf);
    }
}

int robin_api_register(const char *email, const char *password)
{
    char **lines;
    int nline, ret;

    ret = ra_send("register %s %s", email, password);
    if (ret) {
        err("register: could not send the message to the server");
        return -1;
    }

    ret = ra_wait_reply(&lines, &nline);
    if (ret) {
        err("register: could not retrieve the reply from the server");
        return -1;
    }

    dbg("register: reply: %s", lines[0]);

    /* check for errors */
    if (nline < 0)
        return nline;

    return 0;
}

int robin_api_login(const char *email, const char *password)
{
    char **lines;
    int nline, ret;

    ret = ra_send("login %s %s", email, password);
    if (ret) {
        err("login: could not send the message to the server");
        return -1;
    }

    ret = ra_wait_reply(&lines, &nline);
    if (ret) {
        err("login: could not retrieve the reply from the server");
        return -1;
    }

    dbg("login: reply: %s", lines[0]);

    /* check for errors */
    if (nline < 0)
        return nline;

    return 0;
}

int robin_api_logout(void)
{
    char **lines;
    int nline, ret;

    ret = ra_send("logout");
    if (ret) {
        err("logout: could not send the message to the server");
        return -1;
    }

    ret = ra_wait_reply(&lines, &nline);
    if (ret) {
        err("logout: could not retrieve the reply from the server");
        return -1;
    }

    dbg("logout: reply: %s", lines[0]);

    /* check for errors */
    if (nline < 0)
        return nline;

    return 0;
}
