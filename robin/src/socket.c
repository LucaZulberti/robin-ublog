#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "socket.h"
#include "robin.h"

const int log_id = ROBIN_LOG_ID_SOCKET;

/*
 * Local functions
 */

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
    char *ptr = vptr;
    char *end;

    if (!buf)
        return -EINVAL;

    if (!*buf) {		/* must allocate buffer */
        *buf = malloc(sysconf(_SC_PAGESIZE));
        if (!*buf)
            return -ENOMEM;
        *len = 0;	/* buffer exists but within no buffered data */
    }

    nread = 0;
realloc:
    if (n - nread <= 0)
        return -ENOMEM;

    /* Ok, we must read new data in order to hope getting a '\n' char.
     * We append new read data into "*buf".
     */
    *buf = realloc(*buf, *len + n - nread);
    if (!*buf)
        return -ENOMEM;

again:
    nread = read(fd, *buf + *len, n);
    if (nread < 0) {
        if (errno == EAGAIN)
            goto again;
    }
    if (nread == 0)		/* EOF! */
        return 0;

    /* Now let's try to find the '\n'! */
    end = findchar(*buf + *len, '\n', nread);

    *len += nread;
    if (!end)
        goto realloc;

    nstr = end - *buf + 1;
    if (nstr + 1 > n)
        return nstr + 1;	/* cannot store the string into vptr */

    memcpy(vptr, *buf, nstr);	/* copy the '\n' too */
    ptr[nstr + 1] = '\0';		/* add the null-terminate char */

    memmove(*buf, *buf + nstr, *len - nstr); /* update buf data */
    *len -= nstr;

    return nstr;
}

static int readn(int fd, void *vptr, size_t n)
{
    int nleft = n, nread;
    char *ptr = vptr;

    while (nleft > 0) {
        nread = read(fd, ptr, nleft);
        if (nread < 0) {
            if (errno != EAGAIN)
                return -1;
            nread = 0;
        } else if (nread == 0)
            break;

        nleft -= nread;
        ptr += nread;
    }

    return n - nleft;
}

static int writen(int fd, void *vptr, size_t n)
{
    int nleft = n, nwritten;
    char *ptr = vptr;

    while (nleft > 0) {
        nwritten = write(fd, ptr, nleft);
        if (nwritten < 0) {
            if (errno != EAGAIN)
                return -1;
            nwritten = 0;
        } else if (nwritten == 0)
            break;

        nleft -= nwritten;
        ptr += nwritten;
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

int socket_open_listen(char *host, unsigned short port, int *s_listen)
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
        robin_log_err(log_id, "getaddrinfo: %s", gai_strerror(ret));
        goto open_listen_error;
    }

    if (addr == NULL) {
        robin_log_err(log_id, "unable to find host \"%s\"", host);
        goto open_listen_error;
    }

    /* override port */
    ((struct sockaddr_in *) addr->ai_addr)->sin_port = htons(port);

    ret = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (ret < 0) {
        robin_log_err(log_id, "socket: %s", strerror(errno));
        goto open_listen_error;
    }
    *s_listen = ret;

    ret = bind(*s_listen, addr->ai_addr, addr->ai_addrlen);
    if (ret < 0) {
        robin_log_err(log_id, "bind: %s", strerror(errno));
        goto open_listen_error;
    }

    ret = listen(*s_listen, 10);
    if (ret < 0) {
        robin_log_err(log_id, "listen: %s", strerror(errno));
        goto open_listen_error;
    }

    robin_log_info(log_id, "server listening for incoming connections");

    ret = 0;

open_listen_error:
    freeaddrinfo(addr);
    return ret;
}

int socket_accept_connection(int s_listen, int *s_connect)
{
    char host[NI_MAXHOST], service[NI_MAXSERV];
    struct sockaddr_in sock_addr;
    socklen_t sock_addr_len = sizeof(sock_addr);
    int ret;

    ret = accept(s_listen, (struct sockaddr *) &sock_addr, &sock_addr_len);
    if (ret < 0) {
        robin_log_err(log_id, "accept: %s", strerror(errno));
        return -1;
    }
    *s_connect = ret;

    ret = getnameinfo((struct sockaddr *) &sock_addr, sock_addr_len,
                      host, NI_MAXHOST, service, NI_MAXSERV,
                      NI_NUMERICSERV);
    if (ret == 0)
        robin_log_info(log_id, "new client from %s:%s", host, service);
    else
        robin_log_err(log_id, "getnameinfo: %s", gai_strerror(ret));

    return 0;
}

int socket_set_keepalive(int fd, int idle, int intvl, int cnt)
{
    int on = 1;
    int ret;

    /* Set TCP keepalive on */
    ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &on, sizeof(on));
    if (ret < 0) {
        robin_log_err(log_id, "setsocket(SOL_SOCKET): %s", strerror(errno));
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
        robin_log_err(log_id, "setsocket(SOL_TCP/TCP_KEEPIDLE): %s",
                      strerror(errno));
        return -1;
    }
    ret = setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (char *) &intvl, sizeof(intvl));
    if (ret < 0) {
        robin_log_err(log_id, "setsocket(SOL_TCP/TCP_KEEPINTVL): %s",
                      strerror(errno));
        return -1;
    }
    ret = setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (char *) &cnt, sizeof(cnt));
    if (ret < 0) {
        robin_log_err(log_id, "setsocket(SOL_TCP/TCP_KEEPCNT): %s",
                      strerror(errno));
        return -1;
    }

    return 0;
}

int socket_close(int s)
{
    return close(s);
}
