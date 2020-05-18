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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "robin.h"
#include "robin_thread.h"
#include "robin_user.h"
#include "lib/socket.h"


/*
 * Log shortcut
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_MAIN, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_MAIN, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_MAIN, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_MAIN, fmt, ## args)


/*
 * Global data
 */

static char *h_name;

/*
 * Signal handlers
 */

static void sigint_handler(int signum)
{
    (void) signum;

    dbg("SIGINT: robin_thread_pool_free");
    robin_thread_pool_free();
    dbg("SIGINT: robin_user_free_all");
    robin_user_free_all();

    if (h_name) {
        dbg("SIGINT: free: h_name=%p", h_name);
        free(h_name);
    }

    _exit(EXIT_SUCCESS);
}


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
	struct sigaction act;
    int port;
    size_t h_name_len;
    int server_fd, newclient_fd;
    int ret;

    welcome();

    /* register signal handlers */
    act.sa_flags = 0;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGINT, &act, NULL) < 0) {
        err("sigaction: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /*
     * Argument parsing
     */
    if (argc != 3) {
        err("invalid number of arguments.");
        usage();
        exit(EXIT_FAILURE);
    }

    /* save host string */
    h_name_len = strlen(argv[1]);
    h_name = malloc((h_name_len + 1) * sizeof(char));
    if (!h_name) {
        err("%s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    strcpy(h_name, argv[1]);

    /* parse port */
    port = atoi(argv[2]);

	info("local address is %s and port is %d", h_name, port);


    /*
     * Socket creation and listening
     */

    if (socket_open_listen(h_name, port, &server_fd) < 0) {
        err("failed to start the server socket");
        exit(EXIT_FAILURE);
    }

    if (socket_set_keepalive(server_fd, 10, 10, 6) < 0) {
        err("failed to set keepalive socket options");
        exit(EXIT_FAILURE);
    }


    /*
     * Thread pool spawning
     */

    if (robin_thread_pool_init()) {
        err("failed to initialize thread pool!");
        exit(EXIT_FAILURE);
    }


    /*
     * Server loop
     */

    while (1) {
        ret = socket_accept_connection(server_fd, &newclient_fd);
        if (ret < 0) {
            err("failed to accept client connection");
            /* waiting for another client */
            break;
        }

        robin_thread_pool_dispatch(newclient_fd);
    }

    exit(EXIT_SUCCESS);
}
