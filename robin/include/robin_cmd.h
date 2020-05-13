/*
 * robin_cmd.c
 *
 * Header file for Robin Commands
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_CMD_H
#define ROBIN_CMD_H

#include <stddef.h>

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

/* available Robin Commands */
extern robin_cmd_t robin_cmds[];

/**
 * @brief Allocate a robin_ctx_t structure in memory and initialize it.
 *
 * @param log_id        log id
 * @param fd            socket file descriptor associated with the RT
 * @return robin_ctx_t* the allocated context
 */
robin_ctx_t *robin_ctx_alloc(int log_id, int fd);

/**
 * @brief free the memory allocated for the given context.
 *
 * @param ctx the allocated context
 */
void robin_ctx_free(robin_ctx_t *ctx);

/**
 * @brief Reply to the connected client found in given context
 *
 * @param ctx  context for the connection
 * @param fmt  printf-like format string
 * @param ...  printf-like arguments
 * @return int 0 on success, -1 on error
 */
int _robin_reply(robin_ctx_t *ctx, const char *fmt, ...);
#define robin_reply(ctx, fmt, args...) _robin_reply(ctx, fmt "\n", ## args)

#endif /* ROBIN_CMD_H */
