/*
 * linux.h
 *
 * Header file containing function declaration to manage sockets.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef LINUX_H
#define LINUX_H

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int socket_recvline(char **buf, size_t *len, int fd, char *vptr, size_t n);
int socket_recvn(int s, void *vptr, size_t n);
int socket_recv(int s, void *vptr, size_t n);
int socket_sendn(int s, void *vptr, size_t n);

int socket_open_listen(char *host, unsigned short port, int *s_listen);
int socket_accept_connection(int s_listen, int *s_connect);
int socket_set_keepalive(int fd, int idle, int intvl, int cnt);
int socket_close(int s);

#endif /* LINUX_H */
