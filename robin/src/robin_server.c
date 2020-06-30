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
#include <time.h>

#include "robin.h"
#include "robin_cip.h"
#include "robin_thread.h"
#include "robin_user.h"
#include "lib/socket.h"


/*
 * Log shortcuts
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_MAIN, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_MAIN, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_MAIN, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_MAIN, fmt, ## args)


/*
 * Signal handlers
 */

volatile static sig_atomic_t signal_caught;
static void sig_handler(int sig)
{
    signal_caught = sig;
}


/*
 * Print helpers
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

static void usage(void)
{
    puts("usage: robin_server <host> <port>");
    puts("\thost: hostname where the server is executed");
    puts("\tport: port on which the server will listen for incoming "
         "connections");
}


/*
 * Robin Server
 */

int main(int argc, char **argv)
{
    struct sigaction act;
    char *h_name;
    int port;
    int server_fd, newclient_fd;
    int ret;

    welcome();

    /*
     * Register signal handler for freeing up
     * the resources on SIGINT (for Valgrind debug)
     */

    act.sa_flags = 0;
    act.sa_handler = sig_handler;
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

    h_name = argv[1];
    port = atoi(argv[2]);

    info("local address is %s and port is %d", h_name, port);

    /* init seed for prng (needed by lib/password) */
    srand(time(NULL));


    /*
     * Socket creation and listening
     */

    if (socket_open_listen(h_name, port, &server_fd) < 0) {
        err("failed to start the server socket");
        exit(EXIT_FAILURE);
    }


    /*
     * Load users' email and password from file
     */

    if (robin_users_load("./users.txt")) {
        err("failed to load user file from file system!");
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
            /* signal caught, terminate the server on SIGINT */
            if (errno == EINTR && signal_caught == SIGINT)
                break;

            err("failed to accept client connection");
            /* waiting for another client */
            continue;
        }

        robin_thread_pool_dispatch(newclient_fd);
    }


    /*
     * Free resources
     */

    dbg("robin_thread_pool_free");
    robin_thread_pool_free();
    dbg("robin_user_free_all");
    robin_user_free_all();
    dbg("robin_cip_free_all");
    robin_cip_free_all();
    dbg("socket_close");
    socket_close(server_fd);

    exit(EXIT_SUCCESS);
}
