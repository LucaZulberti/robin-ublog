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
 * Robin Manager types and data
 */

#define ROBIN_CMD_LEN 300

typedef enum robin_cmd_retval {
    ROBIN_CMD_ERR = -1,
    ROBIN_CMD_OK = 0,
    ROBIN_CMD_QUIT
} robin_cmd_retval_t;

typedef struct robin_cmd_ctx {
    int log_id;
    int fd;
} robin_cmd_ctx_t;

typedef robin_cmd_retval_t (*robin_cmd_fn_t)(const robin_thread_t *rt,
                                             char *args);

typedef struct robin_cmd {
    char *name;
    char *desc;
    robin_cmd_fn_t fn;
} robin_cmd_t;

static robin_cmd_retval_t
robin_cmd_help(const robin_thread_t *rt, char *args);
static robin_cmd_retval_t
robin_cmd_quit(const robin_thread_t *rt, char *args);

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
#define robin_reply(rt, fmt, args...) socket_send_reply(rt, fmt "\n", ## args)
static int socket_send_reply(const robin_thread_t *rt, const char *fmt, ...)
{
    const int log_id = ROBIN_LOG_ID_RT_BASE + rt->id;
    va_list args, test_args;
    char *reply;
    int reply_len;

    va_start(args, fmt);
    va_copy(test_args, args);

    reply_len = vsnprintf(NULL, 0, fmt, test_args);
    va_end(test_args);

    reply = malloc(reply_len * sizeof(char));
    if (!reply) {
        robin_log_err(log_id, "malloc: %s", strerror(errno));
        return -1;
    }

    if (vsnprintf(reply, reply_len + 1, fmt, args) < 0) {
        robin_log_err(log_id, "vsnprintf: %s", strerror(errno));
        return -1;
    }
    va_end(args);

    if (socket_sendn(rt->fd, reply, reply_len) < 0) {
        robin_log_err(log_id, "socket_sendn: failed to send data to socket");
        free(reply);
        return -1;
    }

    free(reply);
    return 0;
}

/*
 * Local Robin Commands
 */

robin_cmd_retval_t robin_cmd_help(const robin_thread_t *rt, char *args)
{
    (void) args;
    robin_cmd_t *cmd;
    const int ncmds = sizeof(robin_cmds) / sizeof(robin_cmd_t) - 1;

    if (robin_reply(rt, "%d available commands:", ncmds) < 0)
        return ROBIN_CMD_ERR;

    for (cmd = robin_cmds; cmd->name != NULL; cmd++) {
        if (robin_reply(rt, "%s\t%s", cmd->name, cmd->desc) < 0)
            return ROBIN_CMD_ERR;
    }

    return ROBIN_CMD_OK;
}

robin_cmd_retval_t robin_cmd_quit(const robin_thread_t *rt, char *args)
{
    (void) args;

    if (robin_reply(rt, "0 bye bye!") < 0)
        return ROBIN_CMD_ERR;

    return ROBIN_CMD_QUIT;
}


/*
 * Exported functions
 */

void robin_manage_connection(robin_thread_t *rt)
{
    const int log_id = ROBIN_LOG_ID_RT_BASE + rt->id;
    int nread;
    char cmd[ROBIN_CMD_LEN], *args;
    robin_cmd_t *rt_cmd;

    while (1) {
        nread = socket_recvline(&(rt->buf), &(rt->len), rt->fd,
                                cmd, ROBIN_CMD_LEN + 1);
        if (nread < 0) {
            robin_log_err(log_id, "failed to receive a line from the client");
            goto manager_quit;
        } else if (nread == 0){
            robin_log_warn(log_id, "client disconnected");
            goto manager_quit;
        }

        /* blank line */
        if (*cmd == '\n')
            continue;

        args = strchr(cmd, ' ');
        if (args)
            *(args++) = '\0'; /* terminate command name string */
        else
            cmd[nread - 1] = '\0'; /* substitute \n with \0 */

        robin_log_info(log_id, "command received: %s", cmd);

        /* search for the command */
        for (rt_cmd = robin_cmds; rt_cmd->name != NULL; rt_cmd++) {
            if (!strcmp(cmd, rt_cmd->name)) {
                /* execute cmd and evaluate the returned value */
                switch (rt_cmd->fn(rt, args)) {
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

        if (rt_cmd->name == NULL)
            if (robin_reply(rt, "-1 invalid command; type help for the "
                                "list of availble commands") < 0) {
                robin_log_err(log_id, "failed to send invalid command reply");
                goto manager_quit;
            }
    }

manager_quit:
    robin_log_info(log_id, "connection closed");
    socket_close(rt->fd);
}
