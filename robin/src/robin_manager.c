/*
 * robin_manager.c
 *
 * Handles the connection with a client, parsing the incoming commands.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "robin.h"
#include "robin_manager.h"
#include "robin_conn.h"
#include "socket.h"


/*
 * Log shortcut
 */

#define err(fmt, args...)  robin_log_err(log_id, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(log_id, fmt, ## args)
#define info(fmt, args...) robin_log_info(log_id, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(log_id, fmt, ## args)


/*
 * Robin Manager macros
 */

#define ROBIN_MANAGER_BIGCMD_THRESHOLD 5
#define ROBIN_CMD_MAX_LEN 300


/*
 * Exported functions
 */

void robin_manage_connection(int id, int fd)
{
    const int log_id = ROBIN_LOG_ID_RT_BASE + id;
    int nread, big_cmd_count = 0;
    char buf[ROBIN_CMD_MAX_LEN], *args;
    robin_conn_cmd_t *cmd;
    robin_conn_t *conn;

    /* setup the context for this connection */
    conn = robin_conn_alloc(log_id, fd);
    if (!conn)
        goto manager_early_quit;

    while (1) {
        nread = robin_conn_recvline(conn, buf, ROBIN_CMD_MAX_LEN + 1);
        if (nread < 0) {
            err("failed to receive a line from the client");
            goto manager_quit;
        } else if (nread == 0){
            warn("client disconnected");
            goto manager_quit;
        } else if (nread > ROBIN_CMD_MAX_LEN) {
            robin_conn_reply(conn, "-1 command string exceeds "
                STR(ROBIN_CMD_MAX_LEN) " characters: cmd dropped");

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
                switch (cmd->fn(conn, args)) {
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
            if (robin_conn_reply(conn, "-1 invalid command; type help for "
                                 "the list of availble commands") < 0) {
                err("failed to send invalid command reply");
                goto manager_quit;
            }
    }

manager_quit:
    robin_conn_free(conn);
manager_early_quit:
    info("connection closed");
    socket_close(fd);
}
