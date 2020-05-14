/*
 * robin_cmd.c
 *
 * List of Robin commands available for the clients
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "robin.h"
#include "robin_conn.h"
#include "robin_user.h"
#include "socket.h"

/*
 * Log shortcut
 */

#define err(fmt, args...)  robin_log_err(conn->log_id, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(conn->log_id, fmt, ## args)
#define info(fmt, args...) robin_log_info(conn->log_id, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(conn->log_id, fmt, ## args)


/*
 * Local types and macros
 */

struct robin_conn {
    int fd; /* socket file descriptor */

    /* Robin recvline */
    char *buf;
    size_t len;

    /* Robin Log */
    int log_id;

    /* Robin User */
    int logged;
    robin_user_data_t *data;
};

#define ROBIN_CMD_FN(name, conn, args) \
    robin_conn_cmd_ret_t robin_cmd_##name(struct robin_conn *conn, char *args)
#define ROBIN_CMD_FN_DECL(name) static ROBIN_CMD_FN(name,,)
#define ROBIN_CMD_ENTRY(cmd_name, cmd_desc) { \
    .name = #cmd_name,                        \
    .desc = cmd_desc,                         \
    .fn = robin_cmd_##cmd_name                \
}
#define ROBIN_CMD_ENTRY_NULL { .name = NULL, .desc = NULL, .fn = NULL }


/*
 * Local functions
 */

ROBIN_CMD_FN_DECL(register);
ROBIN_CMD_FN_DECL(login);
ROBIN_CMD_FN_DECL(logout);
ROBIN_CMD_FN_DECL(help);
ROBIN_CMD_FN_DECL(quit);


/*
 * Exported data
 */

robin_conn_cmd_t robin_cmds[] = {
    ROBIN_CMD_ENTRY(register, "register to Robin with e-mail and password"),
    ROBIN_CMD_ENTRY(login, "login to Robin with e-mail and password"),
    ROBIN_CMD_ENTRY(logout, "logout from Robin"),
    ROBIN_CMD_ENTRY(help, "print this help"),
    ROBIN_CMD_ENTRY(quit, "terminate the connection with the server"),
    ROBIN_CMD_ENTRY_NULL /* terminator */
};

/*
 * Robin function definitions
 */

ROBIN_CMD_FN(register, conn, args)
{
    char *email = args, *psw, *end;
    int ret;

    dbg("register: args=%s", args);

    psw = strchr(args, ' ');
    if (!psw) {
        robin_conn_reply(conn, "-1 no password provided");
        return ROBIN_CMD_OK;
    }
    *(psw++) = '\0';  /* separate email and psw strings */

    /* terminate psw on first space (if any) */
    end = strchr(psw, ' ');
    if (end)
        *end = '\0';

    dbg("register: email=%s psw=%s", email, psw);

    ret = robin_user_add(email, psw);
    if (ret < 0) {
        robin_conn_reply(conn,
            "-1 could not register the new user into the system");
        return ROBIN_CMD_ERR;
    } else if (ret == 1) {
        robin_conn_reply(conn, "-1 invalid email/password format");
        return ROBIN_CMD_OK;
    } else if (ret == 2) {
        robin_conn_reply(conn, "-1 user %s is already registered", email);
        return ROBIN_CMD_OK;
    }

    robin_conn_reply(conn, "0 user registered successfully");

    return ROBIN_CMD_OK;
}

ROBIN_CMD_FN(login, conn, args)
{
    char *email = args, *psw, *end;
    robin_user_data_t *data;

    dbg("login: args=%s", args);

    if (conn->logged) {
        robin_conn_reply(conn, "-2 already signed-in as %s",
                         conn->data->email);
        return ROBIN_CMD_OK;
    }

    psw = strchr(args, ' ');
    if (!psw) {
        robin_conn_reply(conn, "-1 no password provided");
        return ROBIN_CMD_OK;
    }
    *(psw++) = '\0';  /* separete email and psw strings */

    /* terminate psw on first space (if any) */
    end = strchr(psw, ' ');
    if (end)
        *end = '\0';

    dbg("login: email=%s psw=%s", email, psw);

    switch (robin_user_acquire_data(email, psw, &data)) {
        case -1:
            robin_conn_reply(conn,
                "-1 could not login into the system");
            return ROBIN_CMD_ERR;

        case 1:
            robin_conn_reply(conn,
                "-1 user already logged in from another client");
            return ROBIN_CMD_OK;

        case 2:
            robin_conn_reply(conn, "-1 invalid email/password");
            return ROBIN_CMD_OK;

        case 0:
            conn->logged = 1;
            conn->data = data;
            robin_conn_reply(conn, "0 user logged-in successfully");
            return ROBIN_CMD_OK;

        default:
            robin_conn_reply(conn, "-1 unknown error");
            return ROBIN_CMD_ERR;
    }
}

ROBIN_CMD_FN(logout, conn, args)
{
    (void) args;

    dbg("logout:");

    if (!conn->logged) {
        robin_conn_reply(conn, "-2 login is required before logout");
        return ROBIN_CMD_OK;
    }

    robin_user_release_data(conn->data);
    conn->logged = 0;
    conn->data = NULL;

    robin_conn_reply(conn, "0 logout successfull");

    return ROBIN_CMD_OK;
}

ROBIN_CMD_FN(help, conn, args)
{
    (void) args;
    robin_conn_cmd_t *cmd;
    const int ncmds = sizeof(robin_cmds) / sizeof(robin_conn_cmd_t) - 1;

    dbg("help: ncmds=%d", ncmds);

    if (robin_conn_reply(conn, "%d available commands:", ncmds) < 0)
        return ROBIN_CMD_ERR;

    for (cmd = robin_cmds; cmd->name != NULL; cmd++) {
        if (robin_conn_reply(conn, "%s\t%s", cmd->name, cmd->desc) < 0)
            return ROBIN_CMD_ERR;
    }

    return ROBIN_CMD_OK;
}

ROBIN_CMD_FN(quit, conn, args)
{
    (void) args;

    dbg("quit:");

    if (robin_conn_reply(conn, "0 bye bye!") < 0)
        return ROBIN_CMD_ERR;

    return ROBIN_CMD_QUIT;
}


/*
 * Exported functions
 */

robin_conn_t *robin_conn_alloc(int log_id, int fd)
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

void robin_conn_free(robin_conn_t *conn)
{
    if (conn->buf) {
        dbg("conn_free: buf=%p", conn->buf);
        free(conn->buf);
    }

    if (conn->data) {
        dbg("conn_free: data=%p", conn->data);
        robin_user_release_data(conn->data);
    }

    dbg("conn_free: conn=%p", conn);
    free(conn);
}

int _robin_conn_reply(robin_conn_t *conn, const char *fmt, ...)
{
    va_list args, test_args;
    char *reply;
    int reply_len;

    va_start(args, fmt);
    va_copy(test_args, args);

    reply_len = vsnprintf(NULL, 0, fmt, test_args) + 1;
    va_end(test_args);

    dbg("reply: len=%d", reply_len);

    reply = malloc(reply_len * sizeof(char));
    if (!reply) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    if (vsnprintf(reply, reply_len, fmt, args) < 0) {
        err("vsnprintf: %s", strerror(errno));
        return -1;
    }
    va_end(args);

    dbg("reply: msg=%s", reply);

    /* do not send '\0' in reply */
    if (socket_sendn(conn->fd, reply, reply_len - 1) < 0) {
        err("socket_sendn: failed to send data to socket");
        free(reply);
        return -1;
    }

    free(reply);
    return 0;
}

int robin_conn_recvline(robin_conn_t *conn, char *vptr, size_t n)
{
    int nread;

    nread = socket_recvline(&(conn->buf), &(conn->len), conn->fd, vptr, n);

    if (nread > n)
        conn->len = 0; /* discard the command in buffer */

    return nread;
}
