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
#include <unistd.h>

#include "robin.h"
#include "robin_manager.h"


/*
 * Robin Manager types and data
 */

#define ROBIN_MANAGER_MSG_HUNK_LEN 64

typedef enum robin_cmd_retval {
    ROBIN_CMD_ERR = -1,
    ROBIN_CMD_OK = 0,
    ROBIN_CMD_QUIT
} robin_cmd_retval_t;

typedef struct robin_cmd_ctx {
    int log_id;
    int fd;
} robin_cmd_ctx_t;

typedef robin_cmd_retval_t (*robin_cmd_fn_t)(const robin_cmd_ctx_t *ctx,
                                             char *args);

typedef struct robin_cmd {
    char *name;
    char *usage;
    robin_cmd_fn_t fn;
} robin_cmd_t;

static robin_cmd_retval_t
robin_cmd_help(const robin_cmd_ctx_t *ctx, char *args);
static robin_cmd_retval_t
robin_cmd_quit(const robin_cmd_ctx_t *ctx, char *args);

static robin_cmd_t robin_cmds[] = {
    {
        .name = "help",
        .usage = "",
        .fn = robin_cmd_help
    },
    {
        .name = "quit",
        .usage = "",
        .fn = robin_cmd_quit
    },
    { .name = NULL, .usage = NULL, .fn = NULL } /* terminator */
};



/*
 * Local functions
 */

int sock_readline(const robin_cmd_ctx_t *ctx, char **buf)
{
    const int log_id = ctx->log_id;
    const int fd = ctx->fd;
    int n, index, allocated_hunks;
    char c, *tmp_buf = NULL;

    allocated_hunks = 0;
    index = 0;
    do {
        /* allocate memory if necessary */
        if (index == ROBIN_MANAGER_MSG_HUNK_LEN * allocated_hunks) {
            tmp_buf = realloc(tmp_buf, ROBIN_MANAGER_MSG_HUNK_LEN
                                       * (allocated_hunks + 1));
            if (!tmp_buf) {
                robin_log_err(log_id, "%s", strerror(errno));
                return -1;
            }

            allocated_hunks++;
        }

        /* read a byte */
        n = read(fd, &c, 1);
        if (n < 0) {
            robin_log_err(log_id, "%s", strerror(errno));
            return -1;
        }

        if (c == '\n') {
            /* discard \r in \r\n sequence */
            if (tmp_buf[index - 1] == '\r')
                index--;

            tmp_buf[index] = '\0';

            break;
        }

        tmp_buf[index++] = c;
    } while (1);

    *buf = tmp_buf;

    return index;
}

int writen(const robin_cmd_ctx_t *ctx, const char *msg, int len)
{
    const int log_id = ctx->log_id;
    const int fd = ctx->fd;
    int nwritten, nleft = len;

    while (nleft) {
        nwritten = write(fd, msg, nleft);
        if (nwritten < 0) {
            robin_log_err(log_id, "%s", strerror(errno));
            return -1;
        }

        nleft -= nwritten;
    }

    return 0;
}

static int socket_reply(const robin_cmd_ctx_t *ctx, const char *fmt, ...)
{
    const int log_id = ctx->log_id;
    va_list args, test_args;
    char *reply;
    int reply_len;

    va_start(args, fmt);
    va_copy(test_args, args);

    reply_len = vsnprintf(NULL, 0, fmt, test_args);
    va_end(test_args);

    reply = malloc(reply_len * sizeof(char));
    if (!reply) {
        robin_log_err(log_id, "%s", strerror(errno));
        return -1;
    }

    if (vsnprintf(reply, reply_len + 1, fmt, args) < 0) {
        robin_log_err(log_id, "%s", strerror(errno));
        return -1;
    }
    va_end(args);

    if (writen(ctx, reply, reply_len) < 0) {
        robin_log_err(log_id, "%s", strerror(errno));
        free(reply);
        return -1;
    }

    free(reply);
    return 0;
}

/*
 * Local Robin Commands
 */

robin_cmd_retval_t robin_cmd_help(const robin_cmd_ctx_t *ctx, char *args)
{
    (void) args;
    robin_cmd_t *cmd;
    const int ncmds = sizeof(robin_cmds) / sizeof(robin_cmd_t) - 1;

    if (socket_reply(ctx, "%d available commands:\n", ncmds) < 0)
        return ROBIN_CMD_ERR;

    for (cmd = robin_cmds; cmd->name != NULL; cmd++) {
        if (socket_reply(ctx, "\t- %s %s\n", cmd->name, cmd->usage) < 0)
            return ROBIN_CMD_ERR;
    }

    return ROBIN_CMD_OK;
}

robin_cmd_retval_t robin_cmd_quit(const robin_cmd_ctx_t *ctx, char *args)
{
    (void) args;

    if (socket_reply(ctx, "0 bye bye!\n") < 0)
        return ROBIN_CMD_ERR;

    return ROBIN_CMD_QUIT;
}


/*
 * Exported functions
 */

void robin_manage_connection(int log_id, int fd)
{
    int n;
    char *msg, *args;
    robin_cmd_t *cmd;
    const robin_cmd_ctx_t ctx = {
        .log_id = log_id,
        .fd = fd
    };

    while (1) {
        n = sock_readline(&ctx, &msg);
        if (n < 0) {
            robin_log_err(log_id, "failed to receive a line from the client");
            goto manager_quit;
        }

        /* blank line */
        if (*msg == '\0') {
            free(msg);
            continue;
        }

        args = strchr(msg, ' ');
        /* if there are arguments */
        if (args) {
            *args = '\0'; /* terminate command name string */
            args++;       /* point to the first character in arguments */
        }

        /* search for the command */
        for (cmd = robin_cmds; cmd->name != NULL; cmd++) {
            if (!strcmp(msg, cmd->name)) {
                robin_log_info(log_id, "new command received: %s", cmd->name);
                /* execute cmd and evaluate the returned value */
                switch (cmd->fn(&ctx, args)) {
                    case ROBIN_CMD_OK:
                        break;

                    case ROBIN_CMD_ERR:
                        robin_log_err(log_id,
                            "failed to execute the requested command");
                    case ROBIN_CMD_QUIT:
                        goto manager_quit;
                }
                break;
            }
        }

        if (cmd->name == NULL)
            if (socket_reply(&ctx,
                             "-1 invalid command; type help for the "
                             "list of availble commands\n", msg) < 0) {
                robin_log_err(log_id, "failed to send invalid command reply");
                goto manager_quit;
            }

        free(msg);
    }

manager_quit:
    robin_log_info(log_id, "connection closed");
    free(msg);
    close(fd);
}
