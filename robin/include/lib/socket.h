/*
 * socket.h
 *
 * Header file containing function declaration to manage sockets.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef SOCKET_H
#define SOCKET_H

#include <stddef.h>

int socket_recvline(char **buf, size_t *len, int fd, char *vptr, size_t n);
int socket_recvn(int s, void *vptr, size_t n);
int socket_recv(int s, void *vptr, size_t n);
int socket_sendn(int s, void *vptr, size_t n);

int socket_open_listen(const char *host, unsigned short port, int *s_listen);
int socket_open_connect(const char *host, unsigned short port, int *s_connect);
int socket_accept_connection(int s_listen, int *s_connect);
int socket_set_keepalive(int fd, int idle, int intvl, int cnt);
int socket_close(int s);

#endif /* SOCKET_H */
