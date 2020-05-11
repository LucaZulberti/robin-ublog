/*
 * robin_manager.c
 *
 * Handles the connection with a client, parsing the incoming commands.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "robin.h"
#include "robin_manager.h"
#include "socket.h"


/*
 * Log shortcut
 */

#define err(fmt, args...)  robin_log_err(ctx->log_id, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ctx->log_id, fmt, ## args)
#define info(fmt, args...) robin_log_info(ctx->log_id, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ctx->log_id, fmt, ## args)


/*
 * Robin Manager types and data
 */

#define ROBIN_MANAGER_BIGCMD_THRESHOLD 5
#define ROBIN_CMD_LEN 300

typedef enum robin_cmd_retval {
    ROBIN_CMD_ERR = -1,
    ROBIN_CMD_OK = 0,
    ROBIN_CMD_QUIT
} robin_cmd_retval_t;

typedef struct robin_cmd_ctx {
    /* Robin Manager stuff */
    int fd;
    char *buf;
    size_t len;

    /* Robin Log */
    int log_id;
} robin_ctx_t;

typedef robin_cmd_retval_t (*robin_cmd_fn_t)(robin_ctx_t *ctx, char *args);

typedef struct robin_cmd {
    char *name;
    char *desc;
    robin_cmd_fn_t fn;
} robin_cmd_t;

static robin_cmd_retval_t
robin_cmd_help(robin_ctx_t *ctx, char *args);
static robin_cmd_retval_t
robin_cmd_quit(robin_ctx_t *ctx, char *args);

static robin_cmd_t robin_cmds[] = {
    {
        .name = "help",
        .desc = "print this help",
        .fn = robin_cmd_help
    },
    {
        .name = "quit",
        .desc = "terminate the connection with the server",
        .fn = robin_cmd_quit
    },
    { .name = NULL, .desc = NULL, .fn = NULL } /* terminator */
};


/*
 * Local functions
 */
#define robin_reply(ctx, fmt, args...) socket_send_reply(ctx, fmt "\n", ## args)
static int socket_send_reply(robin_ctx_t *ctx, const char *fmt, ...)
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

/*
 * Local Robin Commands
 */

robin_cmd_retval_t robin_cmd_help(robin_ctx_t *ctx, char *args)
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

robin_cmd_retval_t robin_cmd_quit(robin_ctx_t *ctx, char *args)
{
    (void) args;

    if (robin_reply(ctx, "0 bye bye!") < 0)
        return ROBIN_CMD_ERR;

    return ROBIN_CMD_QUIT;
}


/*
 * Exported functions
 */

void robin_manage_connection(int id, int fd)
{
    const int log_id = ROBIN_LOG_ID_RT_BASE + id;
    int nread, big_cmd_count = 0;
    char buf[ROBIN_CMD_LEN], *args;
    robin_cmd_t *cmd;
    robin_ctx_t *ctx;

    /* setup the context for this connection */
    ctx = malloc(sizeof(robin_ctx_t));
    if (!ctx) {
        /* log shortcut cannot be used here */
        robin_log_err(log_id, "malloc: %s", strerror(errno));
        goto manager_early_quit;
    }
    memset(ctx, 0, sizeof(robin_ctx_t));

    ctx->fd = fd;
    ctx->log_id = log_id;

    while (1) {
        nread = socket_recvline(&(ctx->buf), &(ctx->len), ctx->fd,
                                buf, ROBIN_CMD_LEN + 1);
        if (nread < 0) {
            err("failed to receive a line from the client");
            goto manager_quit;
        } else if (nread == 0){
            warn("client disconnected");
            goto manager_quit;
        } else if (nread > ROBIN_CMD_LEN) {
            robin_reply(ctx, "-1 command string exceeds %d characters; cmd dropped",
                        ROBIN_CMD_LEN);

            /* discard buffer */
            ctx->len = 0;

            /* close connection with client if it is too annoying */
            if (++big_cmd_count >= ROBIN_MANAGER_BIGCMD_THRESHOLD) {
                warn("the client has issued to many oversized commands");
                goto manager_quit;
            }

            continue;
        }

        /* substitute \n or \r\n with \0 */
        if (nread > 1 && buf[nread - 2] == '\r')
            buf[nread - 2] = '\0';
        else
            buf[nread - 1] = '\0';

        /* blank line */
        if (*buf == '\0')
            continue;

        args = strchr(buf, ' ');
        if (args)
            *(args++) = '\0'; /* separate cmd from arguments */

        dbg("command received: %s", buf);

        /* search for the command */
        for (cmd = robin_cmds; cmd->name != NULL; cmd++) {
            if (!strcmp(buf, cmd->name)) {
                info("recognized command: %s", buf);
                /* execute cmd and evaluate the returned value */
                switch (cmd->fn(ctx, args)) {
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
            if (robin_reply(ctx, "-1 invalid command; type help for the "
                                 "list of availble commands") < 0) {
                err("failed to send invalid command reply");
                goto manager_quit;
            }
    }

manager_quit:
    if (ctx->buf)
        free(ctx->buf);
    free(ctx);
manager_early_quit:
    /* log shurtcut cannot be used anymore here */
    robin_log_info(log_id, "connection closed");
    socket_close(fd);
}
