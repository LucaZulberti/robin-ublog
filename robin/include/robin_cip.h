/*
 * robin_cip.c
 *
 * Header file containing the exported interface of Robin Cip module.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_CIP_H
#define ROBIN_CIP_H

typedef struct robin_hashtag {
    const char *tag;
    size_t len;
} robin_hashtag_t;

typedef struct robin_cip {
    time_t ts;
    char *user;
    char *msg;
    robin_hashtag_t *hashtags;
    size_t hashtags_num;

    struct robin_cip *next;
    struct robin_cip *prev;
} robin_cip_t;

/**
 * @brief Add a cip sent by an user to the system
 *
 * @param user name of the user
 * @param msg  cip message
 * @return int 0 on success; -1 on error
 */
int robin_cip_add(const char *user, const char *msg);

/**
 * @brief Get all cips sent after specified timestamp
 *
 * @param ts   timestamp
 * @param cips returned cips
 * @param nums returned number of cips
 * @return int 0 on success; -1 on error
 */
int robin_cip_get_since(time_t ts, const robin_cip_t **cips, unsigned int *nums);

#endif /* ROBIN_CIP_H */
