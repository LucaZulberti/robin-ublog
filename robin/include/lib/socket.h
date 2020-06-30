/*
 * socket.h
 *
 * Header file containing function declaration to manage sockets.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef SOCKET_H
#define SOCKET_H

int socket_recv(int fd, char **buf);
int socket_send(int fd, const void *buf, int n);
int socket_open_listen(const char *host, unsigned short port, int *s_listen);
int socket_open_connect(const char *host, unsigned short port, int *s_connect);
int socket_accept_connection(int s_listen, int *s_connect);
int socket_close(int s);

#endif /* SOCKET_H */
