/*
 * socket.c
 *
 * List of functions to manage Linux sockets.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "robin.h"
#include "lib/socket.h"


/*
 * Log shortcut
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_SOCKET, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_SOCKET, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_SOCKET, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_SOCKET, fmt, ## args)


/*
 * Exported functions
 */

int socket_recv(int fd, char **buf)
{
    char *msg;
    int dim, ret;

    ret = recv(fd, (void *) &dim, sizeof(dim), MSG_WAITALL);
    if (ret == 0)
        return 0;

    if ((ret == -1) || (ret < sizeof(dim)) ) {
        err("recv: %s", ret, strerror(errno));
        return -1;
    }
    dim = ntohl(dim);

    msg = malloc((dim + 1) * sizeof(char));
    if (!msg) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    ret = recv(fd, (void *) msg, dim, MSG_WAITALL);
    if((ret == -1) || (ret < dim)) {
        err("recv: %s", strerror(errno));
        return -1;
    }

    msg[dim] = '\0';

    dbg("packet received, %d bytes: %s", dim, msg);

    *buf = msg;

    return dim;
}

int socket_send(int fd, const void *buf, size_t n)
{
    int dim;
    ssize_t sent;

	dim = htonl(n);

    sent = send(fd, &dim, sizeof(dim), 0);
    if (sent < 0) {
        err("send: %s", strerror(errno));
        return -1;
    }

    sent = send(fd, buf, n, 0);
    if (sent < 0) {
        err("send: %s", strerror(errno));
        return -1;
    }

    dbg("packet sent, %d bytes", n);

    return 0;
}

int socket_open_listen(const char *host, unsigned short port, int *s_listen)
{
    struct addrinfo hints, *addr;
    int ret;

    /* Validate local address where to bind to */
    memset (&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    ret = getaddrinfo(host, NULL, &hints, &addr);
    if (ret) {
        err("getaddrinfo: %s", gai_strerror(ret));
        return ret;
    }

    /* override port */
    ((struct sockaddr_in *) addr->ai_addr)->sin_port = htons(port);

    ret = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (ret < 0) {
        err("socket: %s", strerror(errno));
        goto open_listen_quit;
    }
    *s_listen = ret;

    ret = bind(*s_listen, addr->ai_addr, addr->ai_addrlen);
    if (ret < 0) {
        err("bind: %s", strerror(errno));
        goto open_listen_quit;
    }

    ret = listen(*s_listen, 10);
    if (ret < 0) {
        err("listen: %s", strerror(errno));
        goto open_listen_quit;
    }

    info("server listening for incoming connections");

    ret = 0;

open_listen_quit:
    freeaddrinfo(addr);
    return ret;
}

int socket_accept_connection(int s_listen, int *s_connect)
{
    char host[NI_MAXHOST], service[NI_MAXSERV];
    struct sockaddr_in sock_addr;
    socklen_t sock_addr_len = sizeof(sock_addr);
    int ret, errno_saved;

    ret = accept(s_listen, (struct sockaddr *) &sock_addr, &sock_addr_len);
    if (ret < 0) switch (errno) {
        default:
            errno_saved = errno;
            err("accept: %s", strerror(errno));
            errno = errno_saved;
        case EINTR:
            return -1;
    }
    *s_connect = ret;

    ret = getnameinfo((struct sockaddr *) &sock_addr, sock_addr_len,
                      host, NI_MAXHOST, service, NI_MAXSERV,
                      NI_NUMERICSERV);
    if (ret != 0) {
        err("getnameinfo: %s", gai_strerror(ret));
        return -1;
    }

    info("new client from %s:%s", host, service);

    return ret;
}

int socket_set_keepalive(int fd, int idle, int intvl, int cnt)
{
    int on = 1;
    int ret;

    /* Set TCP keepalive on */
    ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &on, sizeof(on));
    if (ret < 0) {
        err("setsocket(SOL_SOCKET): %s", strerror(errno));
        return -1;
    }
    /* TCP keepalive parameters are:
     *   - first keepalive probe starts after TCP_KEEPIDLE seconds;
     *   - interval between subsequential keepalive probes is TCP_KEEPINTVL
     *     seconds
     *   - the connection is considered dead after TCP_KEEPCNT probes.
     */
    ret = setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (char *) &idle, sizeof(idle));
    if (ret < 0) {
        err("setsocket(SOL_TCP/TCP_KEEPIDLE): %s", strerror(errno));
        return -1;
    }
    ret = setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (char *) &intvl, sizeof(intvl));
    if (ret < 0) {
        err("setsocket(SOL_TCP/TCP_KEEPINTVL): %s", strerror(errno));
        return -1;
    }
    ret = setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (char *) &cnt, sizeof(cnt));
    if (ret < 0) {
        err("setsocket(SOL_TCP/TCP_KEEPCNT): %s", strerror(errno));
        return -1;
    }

    return 0;
}

int socket_open_connect(const char *host, unsigned short port, int *s_connect)
{
    struct addrinfo hints, *addr;
    int ret;

    /* validate remote address where to connect to */
    memset (&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    ret = getaddrinfo(host, NULL, &hints, &addr);
    if (ret) {
        err("getaddrinfo: %s", gai_strerror(ret));
        return ret;
    }

    /* override port */
    ((struct sockaddr_in *) addr->ai_addr)->sin_port = htons(port);

    ret = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (ret < 0) {
        err("socket: %s", strerror(errno));
        goto connect_quit;
    }
    *s_connect = ret;

    ret = connect(*s_connect, (struct sockaddr *)addr->ai_addr,
                  sizeof(*addr->ai_addr));
    if (ret) {
        err("connect: %s", strerror(errno));
        return ret;
    }

    info("connected to %s:%d", host, port);

    ret = 0;

connect_quit:
    freeaddrinfo(addr);
    return ret;
}

int socket_close(int s)
{
    return close(s);
}
