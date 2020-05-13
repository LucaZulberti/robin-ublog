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
};

#define ROBIN_CMD_FN(name, ctx, args) robin_cmd_retval_t robin_cmd_##name(robin_ctx_t *ctx, char *args)
#define ROBIN_CMD_ENTRY(cmd_name, cmd_desc) { \
    .name = #cmd_name,                        \
    .desc = cmd_desc,                         \
    .fn = robin_cmd_##cmd_name                \
}
#define ROBIN_CMD_ENTRY_NULL { .name = NULL, .desc = NULL, .fn = NULL }


/*
 * Local functions
 */

static ROBIN_CMD_FN(register, ctx, args);
static ROBIN_CMD_FN(login, ctx, args);
static ROBIN_CMD_FN(help, ctx, args);
static ROBIN_CMD_FN(quit, ctx, args);


/*
 * Exported data
 */

robin_cmd_t robin_cmds[] = {
    ROBIN_CMD_ENTRY(register, "sign-up to Robin with e-mail and password"),
    ROBIN_CMD_ENTRY(login, "sign-in to Robin with e-mail and password"),
    ROBIN_CMD_ENTRY(help, "print this help"),
    ROBIN_CMD_ENTRY(quit, "terminate the connection with the server"),
    ROBIN_CMD_ENTRY_NULL /* terminator */
};

/*
 * Robin function definitions
 */

ROBIN_CMD_FN(register, ctx, args)
{
    (void) args;

    robin_reply(ctx, "0 register not implemented yet");

    return ROBIN_CMD_OK;
}

ROBIN_CMD_FN(login, ctx, args)
{
    (void) args;

    if (ctx->logged)
        robin_reply(ctx, "0 already signed-in");
    else {
        ctx->logged = 1;
        robin_reply(ctx, "0 fake sign-in successfull");
    }

    return ROBIN_CMD_OK;
}


ROBIN_CMD_FN(help, ctx, args)
{
    (void) args;
    robin_cmd_t *cmd;
    const int ncmds = sizeof(robin_cmds) / sizeof(robin_cmd_t) - 1;

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

    ctx = malloc(sizeof(robin_ctx_t));
    if (!ctx) {
        err("malloc: %s", strerror(errno));
        return NULL;
    }
    memset(ctx, 0, sizeof(robin_ctx_t));

    ctx->fd = fd;
    ctx->log_id = log_id;
    ctx->logged = 0;

    return ctx;
}

void robin_ctx_free(robin_ctx_t *ctx)
{
    if (ctx->buf)
        free(ctx->buf);
    free(ctx);
}

int _robin_reply(robin_ctx_t *ctx, const char *fmt, ...)
{
    va_list args, test_args;
    char *reply;
    int reply_len;

    va_start(args, fmt);
    va_copy(test_args, args);

    reply_len = vsnprintf(NULL, 0, fmt, test_args);
    va_end(test_args);

    reply = malloc(reply_len * sizeof(char));
    if (!reply) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    if (vsnprintf(reply, reply_len + 1, fmt, args) < 0) {
        err("vsnprintf: %s", strerror(errno));
        return -1;
    }
    va_end(args);

    if (socket_sendn(ctx->fd, reply, reply_len) < 0) {
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
