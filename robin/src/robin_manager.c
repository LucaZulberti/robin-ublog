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

typedef robin_cmd_retval_t (*robin_cmd_fn_t)(int fd, char *args);

typedef struct robin_cmd {
    char *name;
    char *usage;
    robin_cmd_fn_t fn;
} robin_cmd_t;

static robin_cmd_retval_t robin_cmd_help(int fd, char *args);
static robin_cmd_retval_t robin_cmd_quit(int fd, char *args);

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

int sock_readline(int fd, char **buf)
{
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
                robin_log_err("%s", strerror(errno));
                return -1;
            }

            allocated_hunks++;
        }

        /* read a byte */
        n = read(fd, &c, 1);
        if (n < 0) {
            robin_log_err("%s", strerror(errno));
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

int writen(int fd, const char *msg, int len)
{
    int nwritten, nleft = len;

    while (nleft) {
        nwritten = write(fd, msg, nleft);
        if (nwritten < 0) {
            robin_log_err("%s", strerror(errno));
            return -1;
        }

        nleft -= nwritten;
    }

    return 0;
}

static int socket_reply(int fd, const char *fmt, ...)
{
    va_list arg, test_arg;
    char *reply;
    int reply_len;

    va_start(arg, fmt);
    va_copy(test_arg, arg);

    reply_len = vsnprintf(NULL, 0, fmt, test_arg);
    va_end(test_arg);

    reply = malloc(reply_len * sizeof(char));
    if (!reply) {
        robin_log_err("%s", strerror(errno));
        return -1;
    }

    if (vsnprintf(reply, reply_len + 1, fmt, arg) < 0) {
        robin_log_err("%s", strerror(errno));
        return -1;
    }
    va_end(arg);

    if (writen(fd, reply, reply_len) < 0) {
        robin_log_err("%s", strerror(errno));
        free(reply);
        return -1;
    }

    free(reply);
    return 0;
}

/*
 * Local Robin Commands
 */

robin_cmd_retval_t robin_cmd_help(int fd, char *args)
{
    (void) args;
    robin_cmd_t *cmd;
    const int ncmds = sizeof(robin_cmds) / sizeof(robin_cmd_t) - 1;

    if (socket_reply(fd, "%d available commands:\n", ncmds) < 0)
        return ROBIN_CMD_ERR;

    for (cmd = robin_cmds; cmd->name != NULL; cmd++) {
        if (socket_reply(fd, "\t- %s %s\n", cmd->name, cmd->usage) < 0)
            return ROBIN_CMD_ERR;
    }

    return ROBIN_CMD_OK;
}

robin_cmd_retval_t robin_cmd_quit(int fd, char *args)
{
    (void) args;

    if (socket_reply(fd, "0 bye bye!\n") < 0)
        return ROBIN_CMD_ERR;

    return ROBIN_CMD_QUIT;
}


/*
 * Exported functions
 */

void robin_manage_connection(int id, int fd)
{
    int n;
    char *msg, *args;
    robin_cmd_t *cmd;

    while (1) {
        n = sock_readline(fd, &msg);
        if (n < 0) {
            robin_log_err("failed to retrieve a line from the client");
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
                robin_log_info("received command: %s", cmd->name);
                /* execute cmd and evaluate the returned value */
                switch (cmd->fn(fd, args)) {
                    case ROBIN_CMD_OK:
                        break;

                    case ROBIN_CMD_ERR:
                        robin_log_err("failed to execute the requested command");
                    case ROBIN_CMD_QUIT:
                        goto manager_quit;
                }
                break;
            }
        }

        if (cmd->name == NULL)
            if (socket_reply(fd, "-1 invalid command; type help for the "
                                 "list of availble commands\n", msg) < 0) {
                robin_log_err("failed to send invalid command reply");
                goto manager_quit;
            }

        free(msg);
    }

manager_quit:
    free(msg);
    close(fd);
}
