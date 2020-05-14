/*
 * robin_conn.c
 *
 * Header file for Robin Connection
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_CMD_H
#define ROBIN_CMD_H

#include <stddef.h>

typedef enum robin_cmd_ret {
    ROBIN_CMD_ERR = -1,
    ROBIN_CMD_OK = 0,
    ROBIN_CMD_QUIT
} robin_conn_cmd_ret_t;

/* hide implementation through typedef */
typedef struct robin_conn robin_conn_t;

typedef struct robin_conn_cmd {
    char *name;
    char *desc;
    robin_conn_cmd_ret_t (*fn)(struct robin_conn *conn, char *args);
} robin_conn_cmd_t;

/* available Robin Commands */
extern robin_conn_cmd_t robin_cmds[];

/**
 * @brief Allocate a robin_conn_t structure in memory and initialize it.
 *
 * @param log_id         log id
 * @param fd             socket file descriptor associated with the RT
 * @return robin_conn_t* the allocated connection
 */
robin_conn_t *robin_conn_alloc(int log_id, int fd);

/**
 * @brief free the memory allocated for the given context.
 *
 * @param conn the allocated connection structure
 */
void robin_conn_free(robin_conn_t *conn);


/**
 * @brief Reply to the connected client
 *
 * @param conn connection to reply to
 * @param fmt  printf-like format string
 * @param ...  printf-like arguments
 * @return int 0 on success, -1 on error
 */
int _robin_conn_reply(robin_conn_t *conn, const char *fmt, ...);
#define robin_conn_reply(conn, fmt, args...) \
    _robin_conn_reply(conn, fmt "\n", ## args)

/**
 * @brief Receive a line from the connected client
 *
 * @param conn connection to receive from
 * @param vptr pointer to the vector where the line will be stored
 * @param n    max number of characters to store in vector
 * @return int 0 on success, -1 on error
 */
int robin_conn_recvline(robin_conn_t *conn, char *vptr, size_t n);

#endif /* ROBIN_CMD_H */
