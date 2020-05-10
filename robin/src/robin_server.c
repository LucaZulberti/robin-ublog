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

#include <stdio.h>

#include "robin.h"
#include "robin_thread.h"
#include "socket.h"


/*
 * Print welcome string
 */
static void welcome(void)
{
    char msg[256];
    int n;

    n = snprintf(msg, 256, "Robin Server %s", ROBIN_RELEASE_STRING);
    puts(msg);

    while (n--)
        putchar('-');
    putchar('\n');
}


/*
 * Usage
 */
static void usage(void)
{
    puts("usage: robin_server <host> <port>");
    puts("\thost: hostname where the server is executed");
    puts("\tport: port on which the server will listen for incoming "
         "connections");
}


/*
 * TCP parameters
 */

int main(int argc, char **argv)
{
    const int log_id = ROBIN_LOG_ID_MAIN;
    int port;
    char *h_name;
    size_t h_name_len;
    int server_fd, newclient_fd;
    int ret;

    welcome();

    /*
     * Argument parsing
     */
    if (argc != 3) {
        robin_log_err(log_id, "invalid number of arguments.");
        usage();
        exit(EXIT_FAILURE);
    }

    /* save host string */
    h_name_len = strlen(argv[1]);
    h_name = malloc((h_name_len + 1) * sizeof(char));
    if (!h_name) {
        robin_log_err(log_id, "%s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    memcpy(h_name, argv[1], h_name_len);

    /* parse port */
    port = atoi(argv[2]);

	robin_log_info(log_id, "local address is %s and port is %d", h_name, port);


    /*
     * Socket creation and listening
     */

    if (socket_open_listen(h_name, port, &server_fd) < 0) {
        robin_log_err(log_id, "failed to start the server socket");
        exit(EXIT_FAILURE);
    }

    if (socket_set_keepalive(server_fd, 10, 10, 6) < 0) {
        robin_log_err(log_id, "failed to set keepalive socket options");
        exit(EXIT_FAILURE);
    }


    /*
     * Thread pool spawning
     */

    if (robin_thread_pool_init()) {
        robin_log_err(log_id, "failed to initialize thread pool!");
        exit(EXIT_FAILURE);
    }


    /*
     * Server loop
     */

    while (1) {
        ret = socket_accept_connection(server_fd, &newclient_fd);
        if (ret < 0) {
            robin_log_err(log_id, "failed to accept client connection");
            /* waiting for another client */
            break;
        }

        robin_thread_pool_dispatch(newclient_fd);
    }

    exit(EXIT_SUCCESS);
}
