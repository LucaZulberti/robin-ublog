/*
 * robin_cip.c
 *
 * Header file containing the exported interface of Robin Cip module.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_CIP_H
#define ROBIN_CIP_H

#include <time.h>

typedef struct robin_cip_exported {
    time_t ts;
    const char *user;
    const char *msg;
} robin_cip_exp_t;

typedef struct robin_hashtag_exported {
    char *tag;
    unsigned int count;
} robin_hashtag_exp_t;


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
 * @param ts    timestamp
 * @param users array of users to filter
 * @param ulen  number of users in the filter
 * @param cips  returned cips
 * @param nums  returned number of cips
 * @return int  0 on success; -1 on error
 */
int robin_cip_get_since(time_t ts, char **users, int ulen, list_t **cips, unsigned int *nums);

/**
 * @brief Get all hashtags sent after specified timestamp
 *
 * @param ts       timestamp
 * @param hashtags returned hashtags
 * @param nums     returned number of hashtags
 * @return int     0 on success; -1 on error
 */
int robin_hashtag_get_since(time_t ts, list_t **hashtags, unsigned int *nums);

/**
 * @brief Free up the resources to terminate gracefully
 *
 * All cips in memory are freed.
 */
void robin_cip_free_all(void);

#endif /* ROBIN_CIP_H */
