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

#define SOCKET_ALLOCATION_CHUNK_LEN  64
#define SOCKET_ALLOCATION_MAX_CHUNKS 1024

#define SOCKET_MAX_BUF_LEN \
    (SOCKET_ALLOCATION_MAX_CHUNKS * SOCKET_ALLOCATION_CHUNK_LEN)


/*
 * Log shortcut
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_SOCKET, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_SOCKET, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_SOCKET, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_SOCKET, fmt, ## args)


/*
 * Local functions
 */

static inline size_t get_allocation_size(int n)
{
    int nblock = (n % SOCKET_ALLOCATION_CHUNK_LEN)
                 ? (n / SOCKET_ALLOCATION_CHUNK_LEN) + 1
                 : (n / SOCKET_ALLOCATION_CHUNK_LEN);

    return sizeof(char) * SOCKET_ALLOCATION_CHUNK_LEN * nblock;
}

static char *findchar(char *s, int c, size_t n)
{
    int i;
    for (i = 0; i < n; i++)
        if (s[i] == '\n')
            break;

    return i == n ? NULL : s + i;
}

/*
 * This function reads an entire line from "fd" storing the result into "vptr".
 * The buffer is null-terminated and includes the newline character.
 *
 * If  "*buf" is NULL, then the function will allocate a buffer for storing
 * the buffered data, which should be freed by the user program.
 * (In  this  case, the value in *n is ignored.)
 *
 * Alternatively, before calling the function, "*buf" can contain a pointer
 * to a malloc(3)-allocated buffer "*len" bytes in size.  If the buffer is not
 * large enough to hold new buffered data, the function resizes it with
 * realloc(3), updating "*buf" and "*len" as necessary.
 *
 * In either case, on a successful call, "*buf" and "*len" will  be  updated
 * to reflect the buffer address and allocated size respectively.
 *
 * On success, the function returns the number of characters read, including
 * the delimiter character, but not including the terminating null
 * byte ('\0'). This value can be used to handle embedded null bytes in the
 * line read.
 *
 * In case of EOF the function returns 0 without touching the buffers.
 *
 * The function returns a negative value (decoding an error) on failure to
 * read a line (including end-of-file condition).
 *
 * In the special case where the read line length (including the '\n' and '\0'
 * chars) is larger than "n" the function returns a value greater than n
 * leaving untouched the "vptr" buffer!
 * The user MUST manage this special case by itself looking into "*buf" for the
 * desired line or by discarding all the data according to its needs.
 */
static int recvline(char **buf, size_t *len, int fd, char *vptr, size_t n)
{
    int nstr, nread;
    char *end;

    if (!buf)
        return -EINVAL;

    nread = 0;
    if (!*buf)
        *len = 0;  /* buffer is not allocated */

    while (1) {
        *buf = realloc(*buf, get_allocation_size(
                                *len + SOCKET_ALLOCATION_CHUNK_LEN));
        if (!*buf) {
            err("realloc: %s", strerror(errno));
            return -1;
        }

        do {
            nread = read(fd, *buf + *len, SOCKET_ALLOCATION_CHUNK_LEN);
            dbg("recvline: nread=%d", nread);
        } while (nread < 0 && errno == EAGAIN);

        if (nread < 0) {
            err("failed to read another chunk from socket");
            return -1;
        } else if (nread == 0)  /* EOF! */
            return 0;

        dbg("recvline: buf=%.*s", *len + nread, *buf);

        /* Now let's try to find the '\n'! */
        end = findchar(*buf + *len, '\n', nread);
        dbg("findchar: end=%p *buf=%p", end, *buf);

        *len += nread;

        if (end)
            break;

        if (*len >= SOCKET_MAX_BUF_LEN) {
            err("recvline: internal buffer exceeds maximum allocation size "
                "(%d)", SOCKET_MAX_BUF_LEN);
            return -1;
        }
    }

    nstr = end - *buf + 1;
    if (nstr + 1 > n) {
        /* line is kept in buffer but not moved in destination */
        warn("recvline: destination buffer too small");
        return nstr + 1;  /* cannot store the string into vptr */
    }

    memcpy(vptr, *buf, nstr);  /* copy the '\n' too */
    vptr[nstr + 1] = '\0';     /* add the terminating null char */

    memmove(*buf, *buf + nstr, *len - nstr); /* update buf data with remaining read bytes */
    *len -= nstr;

    return nstr;
}

static int readn(int fd, void *vptr, size_t n)
{
    int nleft = n, nread;

    while (nleft > 0) {
        do {
            nread = read(fd, vptr, nleft);
        } while (nread < 0 && errno == EAGAIN);

        if (nread < 0) {
            err("read: %s", strerror(errno));
            return -1;
        } else if (nread == 0)
            break;

        nleft -= nread;
        vptr += nread;
    }

    return n - nleft;
}

static int writen(int fd, void *vptr, size_t n)
{
    int nleft = n, nwritten;

    while (nleft > 0) {
        do {
            nwritten = write(fd, vptr, nleft);
        } while (nwritten < 0 && errno == EAGAIN);

        if (nwritten < 0) {
            err("write: %s", strerror(errno));
            return -1;
        } else if (nwritten == 0)
            break;

        nleft -= nwritten;
        vptr += nwritten;
    }

    return n - nleft;
}


/*
 * Exported functions
 */

int socket_recvline(char **buf, size_t *len,
                        int fd, char *vptr, size_t n)
{
    return recvline(buf, len, fd, vptr, n);
}

int socket_recvn(int fd, void *vptr, size_t n)
{
    return readn(fd, vptr, n);
}

int socket_recv(int fd, void *vptr, size_t n)
{
    return read(fd, vptr, n);
}

int socket_sendn(int fd, void *vptr, size_t n)
{
    return writen(fd, vptr, n);
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

    if (connect(*s_connect, (struct sockaddr *)addr->ai_addr,
                sizeof(*addr->ai_addr)) < 0) {
    err("connect: %s", strerror(errno));
    return 1;
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
