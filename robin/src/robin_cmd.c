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
#include "robin_cmd.h"
#include "robin_user.h"
#include "socket.h"

/*
 * Log shortcut
 */

#define err(fmt, args...)  robin_log_err(ctx->log_id, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ctx->log_id, fmt, ## args)
#define info(fmt, args...) robin_log_info(ctx->log_id, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ctx->log_id, fmt, ## args)


/*
 * Local types and macros
 */

struct robin_ctx {
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

#define ROBIN_CMD_FN(name, ctx, args) robin_cmd_retval_t robin_cmd_##name(robin_ctx_t *ctx, char *args)
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

robin_cmd_t robin_cmds[] = {
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

ROBIN_CMD_FN(register, ctx, args)
{
    char *email = args, *psw, *end;
    int ret;

    dbg("register: args=%s", args);

    psw = strchr(args, ' ');
    if (!psw) {
        robin_reply(ctx, "-1 no password provided");
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
        robin_reply(ctx, "-1 could not register the new user into the system");
        return ROBIN_CMD_ERR;
    } else if (ret == 1) {
        robin_reply(ctx, "-1 invalid email/password format");
        return ROBIN_CMD_OK;
    } else if (ret == 2) {
        robin_reply(ctx, "-1 user %s is already registered", email);
        return ROBIN_CMD_OK;
    }

    robin_reply(ctx, "0 user registered successfully");

    return ROBIN_CMD_OK;
}

ROBIN_CMD_FN(login, ctx, args)
{
    char *email = args, *psw, *end;
    robin_user_data_t *data;

    dbg("login: args=%s", args);

    if (ctx->logged) {
        robin_reply(ctx, "-2 already signed-in as %s", ctx->data->email);
        return ROBIN_CMD_OK;
    }

    psw = strchr(args, ' ');
    if (!psw) {
        robin_reply(ctx, "-1 no password provided");
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
            robin_reply(ctx, "-1 could not login the new user to the system");
            return ROBIN_CMD_ERR;

        case 1:
            robin_reply(ctx, "-1 user already logged in from another client");
            return ROBIN_CMD_OK;

        case 2:
            robin_reply(ctx, "-1 invalid email/password");
            return ROBIN_CMD_OK;

        case 0:
            ctx->logged = 1;
            ctx->data = data;
            robin_reply(ctx, "0 user logged-in successfully");
            return ROBIN_CMD_OK;

        default:
            robin_reply(ctx, "-1 unknown error");
            return ROBIN_CMD_ERR;
    }
}

ROBIN_CMD_FN(logout, ctx, args)
{
    (void) args;

    dbg("logout:");

    if (!ctx->logged) {
        robin_reply(ctx, "-2 login is required before logout");
        return ROBIN_CMD_OK;
    }

    robin_user_release_data(ctx->data);
    ctx->logged = 0;
    ctx->data = NULL;

    robin_reply(ctx, "0 logout successfull");

    return ROBIN_CMD_OK;
}

ROBIN_CMD_FN(help, ctx, args)
{
    (void) args;
    robin_cmd_t *cmd;
    const int ncmds = sizeof(robin_cmds) / sizeof(robin_cmd_t) - 1;

    dbg("help: ncmds=%d", ncmds);

    if (robin_reply(ctx, "%d available commands:", ncmds) < 0)
        return ROBIN_CMD_ERR;

    for (cmd = robin_cmds; cmd->name != NULL; cmd++) {
        if (robin_reply(ctx, "%s\t%s", cmd->name, cmd->desc) < 0)
            return ROBIN_CMD_ERR;
    }

    return ROBIN_CMD_OK;
}

ROBIN_CMD_FN(quit, ctx, args)
{
    (void) args;

    dbg("quit:");

    if (robin_reply(ctx, "0 bye bye!") < 0)
        return ROBIN_CMD_ERR;

    return ROBIN_CMD_QUIT;
}


/*
 * Exported functions
 */

robin_ctx_t *robin_ctx_alloc(int log_id, int fd)
{
    robin_ctx_t *ctx;

    ctx = calloc(1, sizeof(robin_ctx_t));
    if (!ctx) {
        err("calloc: %s", strerror(errno));
        return NULL;
    }

    ctx->fd = fd;
    ctx->log_id = log_id;

    return ctx;
}

void robin_ctx_free(robin_ctx_t *ctx)
{
    if (ctx->buf) {
        dbg("ctx_free: buf=%p", ctx->buf);
        free(ctx->buf);
    }

    if (ctx->data) {
        dbg("ctx_free: data=%p", ctx->data);
        robin_user_release_data(ctx->data);
    }

    dbg("ctx_free: ctx=%p", ctx);
    free(ctx);
}

int _robin_reply(robin_ctx_t *ctx, const char *fmt, ...)
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
    if (socket_sendn(ctx->fd, reply, reply_len - 1) < 0) {
        err("socket_sendn: failed to send data to socket");
        free(reply);
        return -1;
    }

    free(reply);
    return 0;
}

int robin_recvline(robin_ctx_t *ctx, char *vptr, size_t n)
{
    int nread;

    nread = socket_recvline(&(ctx->buf), &(ctx->len), ctx->fd, vptr, n);

    if (nread > n)
        ctx->len = 0; /* discard the command in buffer */

    return nread;
}
