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

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "robin.h"
#include "robin_thread.h"


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
static char *host;
static int port;


int main(int argc, char **argv)
{
    size_t host_len;
    int server_fd, newclient_fd;
    struct sockaddr_in serv_addr;
    const int log_id = ROBIN_LOG_ID_MAIN;
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
    host_len = strlen(argv[1]);
    host = malloc((host_len + 1) * sizeof(char));
    if (!host) {
        robin_log_err(log_id, "%s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    memcpy(host, argv[1], host_len);

    /* parse port */
    port = atoi(argv[2]);


    /*
     * Socket creation and listening
     */

    robin_log_info(log_id, "requested host: %s", host);
    robin_log_info(log_id, "requested port: %d", port);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        robin_log_err(log_id, "%s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* init serv_addr structure */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    if (!strcmp(host, "localhost"))
        ret = inet_pton(serv_addr.sin_family,
                        "127.0.0.1", &(serv_addr.sin_addr));
    else
        ret = inet_pton(serv_addr.sin_family, host, &(serv_addr.sin_addr));
    if (!ret) {
        robin_log_err(log_id, "hostname %s is invalid", host);
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_port = htons(port);

    ret = bind(server_fd, (struct sockaddr *) &serv_addr,
               sizeof(serv_addr));
    if (ret < 0) {
        robin_log_err(log_id, "%s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    ret = listen(server_fd, 1);
    if (ret < 0) {
        robin_log_err(log_id, "%s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    robin_log_info(log_id, "server listening on: <%s:%d>",
                   host, port);


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
        newclient_fd = accept(server_fd, NULL, NULL);
        if (newclient_fd < 0) {
            robin_log_err(log_id, "%s", strerror(errno));
            /* waiting for another client */
            break;
        }

        robin_thread_pool_dispatch(newclient_fd);
    }

    exit(EXIT_SUCCESS);
}
