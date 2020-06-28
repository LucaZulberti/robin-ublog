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

typedef struct robin_reply {
    int n;
    void *data;

    /* Used to free content */
    void *free_ptr;
} robin_reply_t;

typedef struct robin_cip {
    time_t ts;
    const char *user;
    const char *msg;

    /* Used to free content */
    void *free_ptr;
} robin_cip_t;

/* Connection handling */
int robin_api_init(int fd);
void robin_api_free(void);

/* API interface */
int robin_api_register(const char *email, const char *password);
int robin_api_login(const char *email, const char *password);
int robin_api_logout(void);
int robin_api_follow(const char *emails, robin_reply_t *reply);
int robin_api_cip(const char *msg);
int robin_api_followers(robin_reply_t *reply);
int robin_api_cips_since(time_t since, robin_reply_t *reply);

#endif /* ROBIN_API_H */
