/*
 * robin_api.h
 *
 * Header file containing public types, macros and interface for
 * Robin API.
 *
 * This file is intended to be included by programs which use librobin_api.a
 * library, like robin_client.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_API_H
#define ROBIN_API_H

/* Connection handling */
int robin_api_init(int fd);
void robin_api_free(void);

/* API interface */
int robin_api_register(const char *email, const char *password);
int robin_api_login(const char *email, const char *password);
int robin_api_logout(void);
int robin_api_follow(const char *emails, int **res);
int robin_api_cip(const char *msg);

#endif /* ROBIN_API_H */
