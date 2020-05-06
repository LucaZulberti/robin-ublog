/*
 * robin_server.c
 *
 * Server for Robin messaging application.
 *
 * It serves incoming connections assigning them an handling thread.
 * N threads are spawned at start-up with N a compile-time constant.
 * N is also the max number of simultaneous connections.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "robin_server.h"
#include "robin.h"

/*
 * TCP parameters
 */
static char *host;
static int port;

/*
 * Robin Threads
 */
static robin_thread_t *robin_thread_pool;

int main(int argc, char **argv)
{
    /* Allocate memory for thread structures */
    robin_thread_pool = malloc(ROBIN_SERVER_THREAD_NUMBER
                               * sizeof(robin_thread_t));
    if (!robin_thread_pool) {
        robin_log_err("%s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* todo */

    exit(EXIT_SUCCESS);
}
