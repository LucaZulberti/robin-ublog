/*
 * robin_client.c
 *
 * CLI client for Robin messaging application.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <stdio.h>
#include <stdlib.h>

#include "robin.h"
#include "robin_api.h"
#include "lib/socket.h"


/*
 * Log shortcuts
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_MAIN, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_MAIN, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_MAIN, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_MAIN, fmt, ## args)


/*
 * Print helpers
 */

static void welcome(void)
{
    char msg[256];
    int n;

    n = snprintf(msg, 256, "Robin Client %s", ROBIN_CLIENT_RELEASE_STRING);
    puts(msg);

    while (n--)
        putchar('-');
    putchar('\n');
}

static void usage(void)
{
    puts("usage: robin_client <host> <port>");
    puts("\thost: remote hostname where the client will try to connect to");
    puts("\tport: remote port");
}


/*
 * Robin Client
 */

int main(int argc, char **argv)
{
    char *h_name;
    int port;
    int client_fd;

    welcome();


    /*
     * Argument parsing
     */

    if (argc != 3) {
        err("invalid number of arguments.");
        usage();
        exit(EXIT_FAILURE);
    }

    h_name = argv[1];
    port = atoi(argv[2]);

    info("remote address is %s and port is %d", h_name, port);


    /*
     * Socket creation and listening
     */

    if (socket_open_connect(h_name, port, &client_fd) < 0) {
        err("failed to connect to the Robin Server");
        exit(EXIT_FAILURE);
    }


    /*
     * Free resources
     */

    dbg("socket_close");
    socket_close(client_fd);

    exit(EXIT_SUCCESS);
}
