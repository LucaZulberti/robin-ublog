/*
 * robin_server.c
 *
 * Server for Robin messaging application.
 *
 * It serves incoming connections assigning them an handling thread using
 * the Robin Thread Pool.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "robin.h"
#include "robin_thread.h"

/*
 * Usage
 */
void usage(void)
{
    puts("usage: robin_server <host> <port>");
    puts("\thost: hostname where the server is executed");
    puts("\tport: port on which the server will listen for incoming "
         "connections");
}

/*
 * TCP parameters
 */
static char *host;
static int port;


int main(int argc, char **argv)
{
    size_t host_len;

    /*
     * Argument parsing
     */
    if (argc != 3) {
        robin_log_err("invalid number of arguments.");
        usage();
        exit(EXIT_FAILURE);
    }

    /* save host string */
    host_len = strlen(argv[1]);
    host = malloc((host_len + 1) * sizeof(char));
    if (!host) {
        robin_log_err("%s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    memcpy(host, argv[1], host_len);

    /* parse port */
    port = atoi(argv[2]);

    robin_log_info("Starting Robin server... (host: %s, port: %d)",
                   host, port);

    /* Thread pool spawning */
    if (robin_thread_pool_init()) {
        robin_log_err("failed thread pool initialization!");
        exit(EXIT_FAILURE);
    }

    /*
     * Socket creation and listening
     */

    /* TODO */


    exit(EXIT_SUCCESS);
}
