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
#include <time.h>

#include "robin.h"
#include "robin_api.h"
#include "lib/socket.h"
#include "lib/utility.h"

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


/*
 * Local data
 */

static int client_fd;
static char *msg_buf = NULL, *reply_buf = NULL;


/*
 * Local functions
 */

static int ra_send(const char *fmt, ...)
{
    va_list args;
    int msg_len;

    va_start(args, fmt);
    msg_len = vsnprintf(NULL, 0, fmt, args);
    if (msg_len < 0) {
        err("vsnprintf: %s", strerror(errno));
        return -1;
    }
    msg_len++;
    va_end(args);

    dbg("ra_send: msg_len=%d", msg_len);

    msg_buf = realloc(msg_buf, msg_len * sizeof(char));
    if (!msg_buf) {
        err("realloc: %s", strerror(errno));
        return -1;
    }

    va_start(args, fmt);
    if (vsnprintf(msg_buf, msg_len, fmt, args) < 0) {
        err("vsnprintf: %s", strerror(errno));
        return -1;
    }
    va_end(args);

    dbg("ra_send: msg_buf=%s", msg_buf);

    /* do not send '\0' in msg */
    if (socket_send(client_fd, msg_buf, msg_len - 1) < 0) {
        err("socket_send: failed to send data to socket");
        return -1;
    }

    return 0;
}

void ra_free_reply(char **reply)
{
    int i = 0;

    while(reply[i]) {
        dbg("free_reply: reply[%d]=%p", i, reply[i]);
        free(reply[i++]);
    }

    dbg("free_reply: reply=%p", reply);
    free(reply);
}

static int ra_wait_reply(char ***replies, int *nrep)
{
    char *buf, **l;
    int n;
    int reply_ret;

    n = socket_recv(client_fd, &buf);
    if (n < 0)
        return -1;

    /* first line contains the number of lines in reply (except the first),
     * or error code if < 0
     */
    reply_ret = strtol(buf, NULL, 10);
    if (reply_ret < 0) {
        l = calloc(2, sizeof(char *));  /* last line is terminator */
    } else {
        l = calloc(reply_ret + 2, sizeof(char *));  /* last line is terminator */
    }

    if (!l) {
        err("calloc: %s", strerror(errno));
        return -1;
    }

    dbg("wait_reply: reply_ret=%d", reply_ret);

    /* always store first packet (it is not counted in nrep) */
    l[0] = buf;

    if (reply_ret > 0) {
        for (int i = 0; i < reply_ret; i++) {
            n = socket_recv(client_fd, &buf);
            if (n < 0) {
                ra_free_reply(l);
                return -1;
            }

            l[i + 1] = buf;
        }
    }

    *replies = l;
    *nrep = reply_ret;

    return 0;
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
    char **replies;
    int nrep, ret;

    dbg("register: email=%s psw=%s", email, password);

    ret = ra_send("register %s %s", email, password);
    if (ret)
        return -1;

    ret = ra_wait_reply(&replies, &nrep);
    if (ret)
        return -1;

    dbg("register: reply: %s", replies[0]);

    ra_free_reply(replies);

    /* check for errors */
    if (nrep < 0)
        return nrep;

    return 0;
}

int robin_api_login(const char *email, const char *password)
{
    char **replies;
    int nrep, ret;

    dbg("login: email=%s psw=%s", email, password);

    ret = ra_send("login %s %s", email, password);
    if (ret)
        return -1;

    ret = ra_wait_reply(&replies, &nrep);
    if (ret)
        return -1;

    dbg("login: reply: %s", replies[0]);

    ra_free_reply(replies);

    /* check for errors */
    if (nrep < 0)
        return nrep;

    return 0;
}

int robin_api_logout(void)
{
    char **replies;
    int nrep, ret;

    dbg("logout");

    ret = ra_send("logout");
    if (ret)
        return -1;

    ret = ra_wait_reply(&replies, &nrep);
    if (ret)
        return -1;

    dbg("logout: reply: %s", replies[0]);

    ra_free_reply(replies);

    /* check for errors */
    if (nrep < 0)
        return nrep;

    return 0;
}

int robin_api_follow(const char *emails, robin_reply_t *reply)
{
    char **replies;
    int nrep, ret;
    int *results;

    dbg("follow: emails=%s", emails);

    ret = ra_send("follow %s", emails);
    if (ret)
        return -1;

    ret = ra_wait_reply(&replies, &nrep);
    if (ret)
        return -1;

    dbg("follow: reply: %s", replies[0]);

    /* check for errors */
    if (nrep < 0)
        return nrep;

    results = malloc(nrep * sizeof(int));
    if (!results) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    for (int i = 0; i < nrep; i++) {
        char *res = strchr(replies[i + 1], ' ');
        *(res++) = '\0';

        results[i] = strtol(res, NULL, 10);

        dbg("follow: user=%s res=%d", replies[i + 1], results[i]);
    }

    reply->n = nrep;
    reply->data = results;

    ra_free_reply(replies);

    return nrep;
}

int robin_api_cip(const char *msg)
{
    char **replies, *msg_to_send, *next;
    char const *last;
    int nrep, len, delta, ret;

    dbg("cip: msg=%s", msg);

    last = msg;
    msg_to_send = NULL;
    len = 0;

    do {
        next = strchr(last, '\n');
        if (next)
            delta = next - last + 1;  /* '\n' -> "\\n" */
        else
            delta = strlen(last);

        msg_to_send = realloc(msg_to_send, len + delta);
        if (!msg_to_send) {
            err("realloc: %s", strerror(errno));
            return -1;
        }

        if (next) {
            memcpy(msg_to_send + len, last, delta - 2);
            memcpy(msg_to_send + len + delta - 2, "\\n", 2);
        } else
            memcpy(msg_to_send + len, last, delta);

        len += delta;

        last = next + 1;
    } while (next);

    ret = ra_send("cip \"%.*s\"", len, msg_to_send);
    if (ret) {
        free(msg_to_send);
        return -1;
    }

    ret = ra_wait_reply(&replies, &nrep);
    if (ret) {
        free(msg_to_send);
        return -1;
    }

    free(msg_to_send);
    ra_free_reply(replies);

    if (nrep < 0)
        return nrep;

    return 0;
}

int robin_api_followers(robin_reply_t *reply)
{
    char **replies, **followers;
    int nrep, ret;

    replies = NULL;

    dbg("followers");

    ret = ra_send("followers");
    if (ret)
        return -1;

    ret = ra_wait_reply(&replies, &nrep);
    if (ret)
        return -1;

    if (nrep < 0)
        return nrep;

    /* free up first line and terminator pointer */
    free(replies[0]);
    free(replies[nrep + 1]);

    followers = malloc(nrep * sizeof(char *));
    if (!followers) {
        err("malloc: %s", strerror(errno));
        ra_free_reply(replies);
        return -1;
    }
    memcpy(followers, &replies[1], nrep * sizeof(char *));

    reply->n = nrep;
    reply->data = followers;

    /* free up the replies array (not the content) */
    free(replies);

    return 0;
}

int robin_api_cips_since(time_t since, robin_reply_t *reply)
{
    robin_cip_t *cs;
    char **replies, **cip_argv;
    int nrep, cip_argc, ret;

    dbg("cips_since: since=%ld", since);

    replies = NULL;

    ret = ra_send("cips_since %ld", since);
    if (ret)
        return -1;

    ret = ra_wait_reply(&replies, &nrep);
    if (ret)
        return -1;

    dbg("nrep=%d", nrep);

    if (nrep < 0)
        return nrep;

    /* free up first line and terminator pointer */
    free(replies[0]);
    free(replies[nrep + 1]);

    cs = malloc(nrep * sizeof(robin_cip_t));
    if (!cs) {
        err("malloc: %s", strerror(errno));
        ra_free_reply(replies);
        return -1;
    }

    for (int i = 0; i < nrep; i++) {
        cip_argv = NULL;

        if (argv_parse(replies[i + 1], &cip_argc, &cip_argv) < 0) {
            err("argv_parse: failed to parse the reply");
            ra_free_reply(replies);
            free(cs);
            return -1;
        }

        cs[i].ts = strtol(cip_argv[0], NULL, 10);
        cs[i].user = cip_argv[1];
        cs[i].msg = cip_argv[2];
        cs[i].free_ptr = cip_argv[0];

        /* free up the argv array (not the content) */
        free(cip_argv);
    }

    reply->n = nrep;
    reply->data = cs;

    /* free up the replies array (not the content) */
    free(replies);

    return 0;
}

int robin_api_hashtags_since(time_t since, robin_reply_t *reply)
{
    robin_hashtag_t *hs;
    char **replies, **ht_argv;
    int nrep, ht_argc, ret;

    dbg("hashtags_since: since=%ld", since);

    replies = NULL;

    ret = ra_send("hashtags_since %ld", since);
    if (ret)
        return -1;

    ret = ra_wait_reply(&replies, &nrep);
    if (ret)
        return -1;

    if (nrep < 0)
        return nrep;

    /* free up first line and terminator pointer */
    free(replies[0]);
    free(replies[nrep + 1]);

    hs = malloc(nrep * sizeof(robin_hashtag_t));
    if (!hs) {
        err("malloc: %s", strerror(errno));
        ra_free_reply(replies);
        return -1;
    }

    for (int i = 0; i < nrep; i++) {
        ht_argv = NULL;

        if (argv_parse(replies[i + 1], &ht_argc, &ht_argv) < 0) {
            err("argv_parse: failed to parse the reply");
            ra_free_reply(replies);
            free(hs);
            return -1;
        }

        hs[i].tag = ht_argv[0];
        hs[i].count = strtol(ht_argv[1], NULL, 10);
        hs[i].free_ptr = ht_argv[0];

        /* free up the argv array (not the content) */
        free(ht_argv);
    }

    reply->n = nrep;
    reply->data = hs;

    /* free up the replies array (not the content) */
    free(replies);

    return 0;
}

int robin_api_quit(void)
{
    char **replies;
    int nrep, ret;

    dbg("quit");

    ret = ra_send("quit");
    if (ret)
        return -1;

    ret = ra_wait_reply(&replies, &nrep);
    if (ret)
        return -1;

    dbg("quit: reply: %s", replies[0]);

    ra_free_reply(replies);

    /* check for errors */
    if (nrep < 0)
        return nrep;

    return 0;
}
