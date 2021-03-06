/*
 * robin_conn.c
 *
 * Handles the connection with a client executing the available Robin Commands
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "robin.h"
#include "robin_cip.h"
#include "robin_conn.h"
#include "robin_user.h"
#include "lib/socket.h"
#include "lib/utility.h"


/*
 * Log shortcuts
 */

#define err(fmt, args...)  robin_log_err(conn->log_id, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(conn->log_id, fmt, ## args)
#define info(fmt, args...) robin_log_info(conn->log_id, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(conn->log_id, fmt, ## args)


/*
 * Local types and macros
 */

#define ROBIN_CONN_MAX 64
#define ROBIN_CONN_BIGCMD_THRESHOLD 5
#define ROBIN_CONN_CMD_MAX_LEN 300
#define ROBIN_CONN_CIP_MAX_LEN 280

typedef enum robin_conn_cmd_ret {
    ROBIN_CMD_ERR = -1,
    ROBIN_CMD_OK = 0,
    ROBIN_CMD_QUIT
} robin_conn_cmd_ret_t;

typedef struct robin_conn {
    int fd; /* socket file descriptor */

    /* Robin reply */
    char *reply;

    /* Robin Log */
    int log_id;

    /* Robin Command */
    int argc;
    char **argv;

    /* Robin User */
    int logged;
    int uid;
} robin_conn_t;

typedef struct robin_conn_cmd {
    char *name;
    char *usage;
    char *desc;
    robin_conn_cmd_ret_t (*fn)(robin_conn_t *conn);
} robin_conn_cmd_t;

#define ROBIN_CONN_CMD_FN(name, conn) \
    robin_conn_cmd_ret_t rc_cmd_##name(robin_conn_t *conn)
#define ROBIN_CONN_CMD_FN_DECL(name) static ROBIN_CONN_CMD_FN(name,)
#define ROBIN_CONN_CMD_ENTRY(cmd_name, cmd_usage, cmd_desc) { \
    .name = #cmd_name,                                        \
    .usage = cmd_usage,                                       \
    .desc = cmd_desc,                                         \
    .fn = rc_cmd_##cmd_name                                   \
}
#define ROBIN_CONN_CMD_ENTRY_NULL { \
    .name = NULL,                   \
    .usage = NULL,                  \
    .desc = NULL,                   \
    .fn = NULL                      \
}


/*
 * Robin Command functions declaration
 */

ROBIN_CONN_CMD_FN_DECL(help);
ROBIN_CONN_CMD_FN_DECL(register);
ROBIN_CONN_CMD_FN_DECL(login);
ROBIN_CONN_CMD_FN_DECL(logout);
ROBIN_CONN_CMD_FN_DECL(follow);
ROBIN_CONN_CMD_FN_DECL(unfollow);
ROBIN_CONN_CMD_FN_DECL(following);
ROBIN_CONN_CMD_FN_DECL(followers);
ROBIN_CONN_CMD_FN_DECL(cip);
ROBIN_CONN_CMD_FN_DECL(cips_since);
ROBIN_CONN_CMD_FN_DECL(hashtags_since);
ROBIN_CONN_CMD_FN_DECL(quit);


/*
 * Local data
 */

static robin_conn_cmd_t robin_cmds[] = {
    ROBIN_CONN_CMD_ENTRY(help, "",
                         "print this help"),
    ROBIN_CONN_CMD_ENTRY(register, "<email> <password>",
                         "register to Robin with email and password"),
    ROBIN_CONN_CMD_ENTRY(login, "<email> <password>",
                         "login to Robin with email and password"),
    ROBIN_CONN_CMD_ENTRY(logout, "",
                         "logout from Robin"),
    ROBIN_CONN_CMD_ENTRY(follow, "<email>",
                         "follow the user identified by the email"),
    ROBIN_CONN_CMD_ENTRY(unfollow, "<email>",
                         "unfollow the user identified by the email"),
    ROBIN_CONN_CMD_ENTRY(following, "",
                         "list following users"),
    ROBIN_CONN_CMD_ENTRY(followers, "",
                         "list followers users"),
    ROBIN_CONN_CMD_ENTRY(cip, "<msg string>",
                         "cip a message to Robin"),
    ROBIN_CONN_CMD_ENTRY(cips_since, "<ts>",
                         "return the cips sent after timestamp"),
    ROBIN_CONN_CMD_ENTRY(hashtags_since, "<ts>",
                         "return the hastags found in cips sent after timestamp"),
    ROBIN_CONN_CMD_ENTRY(quit, "",
                         "terminate the connection with the server"),
    ROBIN_CONN_CMD_ENTRY_NULL /* terminator */
};

/* for gracefully termination */
static robin_conn_t *robin_conns[ROBIN_CONN_MAX];


/*
 * Local functions
 */

static robin_conn_t *rc_alloc(int log_id, int fd)
{
    robin_conn_t *conn;

    conn = calloc(1, sizeof(robin_conn_t));
    if (!conn) {
        err("calloc: %s", strerror(errno));
        return NULL;
    }

    conn->fd = fd;
    conn->log_id = log_id;

    return conn;
}

static void rc_free(robin_conn_t *conn)
{
    if (conn->reply) {
        dbg("conn_free: reply=%p", conn->reply);
        free(conn->reply);
    }

    if (conn->argv) {
        dbg("conn_free: argv=%p", conn->argv);
        free(conn->argv);
    }

    dbg("conn_free: conn=%p", conn);
    free(conn);
}

static int rc_reply(robin_conn_t *conn, const char *fmt, ...)
{
    va_list args;
    int reply_len;

    va_start(args, fmt);
    reply_len = vsnprintf(NULL, 0, fmt, args);
    if (reply_len < 0) {
        err("vsnprintf: %s", strerror(errno));
        return -1;
    }
    reply_len++;
    va_end(args);

    dbg("reply: len=%d", reply_len);

    conn->reply = realloc(conn->reply, reply_len * sizeof(char));
    if (!conn->reply) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    va_start(args, fmt);
    if (vsnprintf(conn->reply, reply_len, fmt, args) < 0) {
        err("vsnprintf: %s", strerror(errno));
        return -1;
    }
    va_end(args);

    dbg("reply: msg=%s", conn->reply);

    /* do not send '\0' in reply */
    if (socket_send(conn->fd, conn->reply, reply_len - 1) < 0) {
        err("socket_send: failed to send data to socket");
        return -1;
    }

    return 0;
}


/*
 * Robin Command function definitions
 */

ROBIN_CONN_CMD_FN(help, conn)
{
    robin_conn_cmd_t *cmd;
    const int ncmds = sizeof(robin_cmds) / sizeof(robin_conn_cmd_t) - 1;

    dbg("%s: ncmds=%d", conn->argv[0], ncmds);

    if (conn->argc != 1) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    if (rc_reply(conn, "%d available commands:", ncmds) < 0)
        return ROBIN_CMD_ERR;

    for (cmd = robin_cmds; cmd->name != NULL; cmd++) {
        if (rc_reply(conn, "%s %s\t%s", cmd->name, cmd->usage, cmd->desc) < 0)
            return ROBIN_CMD_ERR;
    }

    return ROBIN_CMD_OK;
}

ROBIN_CONN_CMD_FN(register, conn)
{
    char *email, *psw;
    int ret;

    dbg("%s", conn->argv[0]);

    if (conn->argc != 3) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    email = conn->argv[1];
    psw = conn->argv[2];

    dbg("%s: email=%s psw=%s", conn->argv[0], email, psw);

    ret = robin_user_add(email, psw);
    if (ret < 0) {
        rc_reply(conn, "-1 could not register the new user into the system");
        return ROBIN_CMD_ERR;
    } else if (ret == 1) {
        rc_reply(conn, "-2 invalid email/password format");
        return ROBIN_CMD_OK;
    } else if (ret == 2) {
        rc_reply(conn, "-3 user %s is already registered", email);
        return ROBIN_CMD_OK;
    }

    rc_reply(conn, "0 user registered successfully");

    return ROBIN_CMD_OK;
}

ROBIN_CONN_CMD_FN(login, conn)
{
    char *email, *psw;
    int uid;

    dbg("%s", conn->argv[0]);

    if (conn->argc != 3) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    email = conn->argv[1];
    psw = conn->argv[2];

    dbg("%s: email=%s psw=%s", conn->argv[0], email, psw);

    if (conn->logged) {
        rc_reply(conn, "-2 already signed-in as %s",
                       robin_user_email_get(conn->uid));
        return ROBIN_CMD_OK;
    }

    switch (robin_user_acquire(email, psw, &uid)) {
        case -1:
            rc_reply(conn, "-1 could not login into the system");
            return ROBIN_CMD_ERR;

        case 0:
            conn->logged = 1;
            conn->uid = uid;
            rc_reply(conn, "0 user logged-in successfully");
            return ROBIN_CMD_OK;

        case 1:
            rc_reply(conn, "-3 user already logged in from another client");
            return ROBIN_CMD_OK;

        case 2:
            rc_reply(conn, "-4 invalid email");
            return ROBIN_CMD_OK;

        case 3:
            rc_reply(conn, "-5 invalid password");
            return ROBIN_CMD_OK;

        default:
            rc_reply(conn, "-6 unknown error");
            return ROBIN_CMD_ERR;
    }
}

ROBIN_CONN_CMD_FN(logout, conn)
{
    dbg("%s", conn->argv[0]);

    if (!conn->logged) {
        rc_reply(conn, "-2 login is required before logout");
        return ROBIN_CMD_OK;
    }

    if (conn->argc != 1) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    robin_user_release(conn->uid);
    conn->logged = 0;

    rc_reply(conn, "0 logout successfull");

    return ROBIN_CMD_OK;
}

ROBIN_CONN_CMD_FN(follow, conn)
{
    char **replies;
    int n, nleft, err;

    n = conn->argc - 1;

    dbg("%s: n_emails=%d", conn->argv[0], n);

    if (!conn->logged) {
        rc_reply(conn, "-2 you must be logged in");
        return ROBIN_CMD_OK;
    }

    if (conn->argc < 2) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    replies = malloc(n * sizeof(char *));
    if (!replies) {
        err("malloc: %s", strerror(errno));
        return ROBIN_CMD_ERR;
    }

    err = 0;
    nleft = n;
    while (nleft && !err) {
        const char *email = conn->argv[n - nleft + 1];

        switch (robin_user_follow(conn->uid, email)) {
            case -1:
                replies[n - nleft] = "-1 could not follow the user";
                err = 1;
                break;

            case 0:
                replies[n - nleft] = "0 user followed";
                break;

            case 1:
                replies[n - nleft] = "1 user does not exist";
                break;

            case 2:
                replies[n - nleft] = "2 user already followed";
                break;

            default:
                replies[n - nleft] = "3 unknown error";
                err = 1;
                break;
        }

        nleft--;
    }

    if (nleft == n) {
        rc_reply(conn, "-1 could not follow any user");
    } else {
        rc_reply(conn, "%d users tried to follow", n - nleft);
        for (int i = 0; i < n - nleft; i++) {
            const char *email = conn->argv[i + 1];
            rc_reply(conn, "%s %s", email, replies[i]);
        }
    }

    free(replies);

    if (!nleft)
        return ROBIN_CMD_OK;
    else
        return ROBIN_CMD_ERR;
}

ROBIN_CONN_CMD_FN(unfollow, conn)
{
    char **replies;
    int n, nleft, err;

    n = conn->argc - 1;

    dbg("%s: n_emails=%d", conn->argv[0], n);

    if (!conn->logged) {
        rc_reply(conn, "-2 you must be logged in");
        return ROBIN_CMD_OK;
    }

    if (conn->argc < 2) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    replies = malloc(n * sizeof(char *));
    if (!replies) {
        err("malloc: %s", strerror(errno));
        return ROBIN_CMD_ERR;
    }

    err = 0;
    nleft = n;
    while (nleft && !err) {
        const char *email = conn->argv[n - nleft + 1];

        switch (robin_user_unfollow(conn->uid, email)) {
            case -1:
                replies[n - nleft] = "-1 could not unfollow the user";
                err = 1;
                break;

            case 0:
                replies[n - nleft] = "0 user unfollowed";
                break;

            case 1:
                replies[n - nleft] = "-1 user is not followed";
                break;

            default:
                replies[n - nleft] = "-1 unknown error";
                err = 1;
                break;
        }

        nleft--;
    }

    if (nleft == n) {
        rc_reply(conn, "-1 could not unfollow any user");
    } else {
        rc_reply(conn, "%d users tried to unfollow", n - nleft);
        for (int i = 0; i < n - nleft; i++) {
            const char *email = conn->argv[i + 1];
            rc_reply(conn, "%s %s", email, replies[i]);
        }
    }

    free(replies);

    if (!nleft)
        return ROBIN_CMD_OK;
    else
        return ROBIN_CMD_ERR;
}

ROBIN_CONN_CMD_FN(following, conn)
{
    char **following;
    size_t len;

    dbg("%s", conn->argv[0]);

    if (!conn->logged) {
        rc_reply(conn, "-2 you must be logged in");
        return ROBIN_CMD_OK;
    }

    if (conn->argc != 1) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    if (robin_user_following_get(conn->uid, &following, &len) < 0) {
        rc_reply(conn, "-1 could not get the list of following users");
        return ROBIN_CMD_ERR;
    }

    rc_reply(conn, "%d users", len);
    for (int i = 0; i < len; i++)
        rc_reply(conn, "%s", following[i]);

    free(following);

    return ROBIN_CMD_OK;
}

ROBIN_CONN_CMD_FN(followers, conn)
{
    char **followers;
    size_t len;

    dbg("%s", conn->argv[0]);

    if (!conn->logged) {
        rc_reply(conn, "-2 you must be logged in");
        return ROBIN_CMD_OK;
    }

    if (conn->argc != 1) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    if (robin_user_followers_get(conn->uid, &followers, &len) < 0) {
        rc_reply(conn, "-1 could not get the list of followers users");
        return ROBIN_CMD_ERR;
    }

    rc_reply(conn, "%d users", len);
    for (int i = 0; i < len; i++)
        rc_reply(conn, "%s", followers[i]);

    free(followers);

    return ROBIN_CMD_OK;
}

ROBIN_CONN_CMD_FN(cip, conn)
{
    const char *user, *msg;

    dbg("%s", conn->argv[0]);

    if (!conn->logged) {
        rc_reply(conn, "-2 you must be logged in");
        return ROBIN_CMD_OK;
    }

    if (conn->argc != 2) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    msg = conn->argv[1];

    if (strlen(msg) > ROBIN_CONN_CIP_MAX_LEN) {
        rc_reply(conn, "-1 cip messages cannot be longer than " STR(ROBIN_CONN_CIP_MAX_LEN)
             " characters");
        return ROBIN_CMD_OK;
    }

    dbg("%s: msg_len=%d", conn->argv[0], strlen(msg));

    user = robin_user_email_get(conn->uid);
    if (!user) {
        err("%s: failed to get user email", conn->argv[0]);
        return ROBIN_CMD_ERR;
    }

    if (robin_cip_add(user, msg) < 0) {
        err("%s: failed to add the cip to the system", conn->argv[0]);
        return ROBIN_CMD_ERR;
    }

    rc_reply(conn, "0 success");

    return ROBIN_CMD_OK;
}

ROBIN_CONN_CMD_FN(cips_since, conn)
{
    char **following;
    size_t foll_len;
    list_t *cip_list, *tmp;
    unsigned int cips_num;
    const robin_cip_exp_t *cip;
    time_t ts;

    dbg("%s", conn->argv[0]);

    if (!conn->logged) {
        rc_reply(conn, "-2 you must be logged in");
        return ROBIN_CMD_OK;
    }

    if (conn->argc != 2) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    ts = strtol(conn->argv[1], NULL, 10);

    dbg("%s: ts=%d", conn->argv[0], ts);

    if (robin_user_following_get(conn->uid, &following, &foll_len) < 0) {
        rc_reply(conn, "-1 could not get the list of following users");
        return ROBIN_CMD_ERR;
    }

    if (robin_cip_get_since(ts, following, foll_len, &cip_list, &cips_num) < 0) {
        err("%s: failed to get the cips", conn->argv[0]);
        return ROBIN_CMD_ERR;
    }

    free(following);

    rc_reply(conn, "%d cips", cips_num);
    for (int i = 0; i < cips_num; i++) {
        cip = (const robin_cip_exp_t *) cip_list->ptr;
        rc_reply(conn, "%d %s \"%s\"", cip->ts, cip->user, cip->msg);

        tmp = cip_list;
        cip_list = cip_list->next;

        free(tmp->ptr);
        free(tmp);
    }

    return ROBIN_CMD_OK;
}

ROBIN_CONN_CMD_FN(hashtags_since, conn)
{
    list_t *hashtag_list, *tmp;
    unsigned int hashtag_num;
    robin_hashtag_exp_t *hashtag;
    time_t ts;

    dbg("%s", conn->argv[0]);

    if (!conn->logged) {
        rc_reply(conn, "-2 you must be logged in");
        return ROBIN_CMD_OK;
    }

    if (conn->argc != 2) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    ts = strtol(conn->argv[1], NULL, 10);

    dbg("%s: ts=%d", conn->argv[0], ts);

    if (robin_hashtag_get_since(ts, &hashtag_list, &hashtag_num) < 0) {
        err("%s: failed to get the cips", conn->argv[0]);
        return ROBIN_CMD_ERR;
    }

    rc_reply(conn, "%d hashtags", hashtag_num);
    for (int i = 0; i < hashtag_num; i++) {
        hashtag = (robin_hashtag_exp_t *) hashtag_list->ptr;
        rc_reply(conn, "%s %d", hashtag->tag, hashtag->count);

        tmp = hashtag_list;
        hashtag_list = hashtag_list->next;

        free(hashtag->tag);
        free(hashtag);
        free(tmp);
    }

    return ROBIN_CMD_OK;
}

ROBIN_CONN_CMD_FN(quit, conn)
{
    dbg("%s", conn->argv[0]);

    if (conn->argc != 1) {
        rc_reply(conn, "-1 invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    if (rc_reply(conn, "0 bye bye!") < 0)
        return ROBIN_CMD_ERR;

    return ROBIN_CMD_QUIT;
}


/*
 * Exported functions
 */

/* redefine log shortcuts for the manager */
#undef err
#undef warn
#undef info
#undef dbg
#define err(fmt, args...)  robin_log_err(log_id, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(log_id, fmt, ## args)
#define info(fmt, args...) robin_log_info(log_id, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(log_id, fmt, ## args)

void robin_conn_manage(int id, int fd)
{
    const int log_id = ROBIN_LOG_ID_RT_BASE + id;
    int nread, big_cmd_count = 0;
    char *buf;
    robin_conn_cmd_t *cmd;
    robin_conn_t *conn;

    /* setup the context for this connection */
    conn = rc_alloc(log_id, fd);
    if (!conn)
        goto manager_early_quit;

    /* access is exclusive due to unique id */
    robin_conns[id] = conn;

    while (1) {
        nread = socket_recv(conn->fd, &buf);
        if (nread < 0) {
            err("failed to receive a line from the client");
            goto manager_quit;
        } else if (nread == 0) {
            warn("client disconnected");
            goto manager_quit;
        } else if (nread > ROBIN_CONN_CMD_MAX_LEN) {
            free(buf);

            rc_reply(conn, "-1 command string exceeds " \
                     STR(ROBIN_CONN_CMD_MAX_LEN) " characters: cmd dropped");

            /* close connection with client if it is too annoying */
            if (++big_cmd_count >= ROBIN_CONN_BIGCMD_THRESHOLD) {
                warn("the client has issued to many oversized commands");
                goto manager_quit;
            }

            continue;
        }

        dbg("command received: %s", buf);

        /* parse the command in argc-argv form and store it in conn */
        if (argv_parse(buf, &conn->argc, &conn->argv) < 0) {
            err("argv_parse: failed to parse command");
            goto manager_quit;
        }

        /* blank line */
        if (conn->argc < 1)
            continue;

        /* search for the command */
        for (cmd = robin_cmds; cmd->name != NULL; cmd++) {
            if (!strcmp(conn->argv[0], cmd->name)) {
                info("recognized command: %s", buf);
                /* execute cmd and evaluate the returned value */
                switch (cmd->fn(conn)) {
                    case ROBIN_CMD_OK:
                        break;

                    case ROBIN_CMD_ERR:
                        err("failed to execute the requested command");
                    case ROBIN_CMD_QUIT:
                        goto manager_quit;
                }
                break;
            }
        }

        if (cmd->name == NULL)
            if (rc_reply(conn, "-1 invalid command; type help for the list of "
                               "availble commands") < 0) {
                err("failed to send invalid command reply");
                goto manager_quit;
            }

        free(buf);
    }

manager_quit:
    if (conn->logged)
        robin_user_release(conn->uid);
    rc_free(conn);
    robin_conns[id] = NULL;
manager_early_quit:
    info("connection closed");
    socket_close(fd);
}

void robin_conn_terminate(int id)
{
    robin_conn_t *conn = robin_conns[id];

    if (conn) {
        if (conn->logged)
            robin_user_release(conn->uid);
        socket_close(conn->fd);
        rc_free(conn);
    }
}
