/*
 * robin_user.c
 *
 * This file contains the functions to handle user information stored in
 * memory and on the file system.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "robin.h"
#include "robin_user.h"


/*
 * Log shortcut
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_USER, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_USER, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_USER, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_USER, fmt, ## args)


/*
 * Local types and macros
 */

typedef struct robin_user {
    robin_user_data_t *data; /* user data */
    pthread_mutex_t acquired; /* exclusive access to user data */
} robin_user_t;


/*
 * Local data
 */

static robin_user_t *users = NULL;
static int users_len = 0;
static pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * Local functions
 */

static int robin_user_add_unsafe(const char * email, const char * psw)
{
    robin_user_t *_users, *new_user;
    robin_user_data_t *data;
    int i;

    dbg("add: email=%s psw=%s", email, psw);

    if (strlen(email) > ROBIN_USER_EMAIL_LEN - 1) {
        warn("add: email is longer than " STR(ROBIN_USER_EMAIL_LEN)
             " characters");
        return 1;
    }

    for (i = 0; i < users_len; i++)
        if (!strncmp(email, users[i].data->email, ROBIN_USER_EMAIL_LEN))
            break;

    if (i < users_len) {  /* email already used */
        warn("add: user %s already registered", email);
        return 2;
    }

    dbg("add: user not existing, allocating data...");
    data = malloc(sizeof(robin_user_data_t));
    if (!data) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    strcpy(data->email, email);
    strcpy(data->psw, psw);
    dbg("add: data allocated and initialized");

    _users = realloc(users, (users_len + 1) * sizeof(robin_user_t));
    if (!_users) {
        err("realloc: %s", strerror(errno));
        return -1;
    }
    users = _users;

    new_user = &users[users_len];
    new_user->data = data;
    new_user->data->uid = users_len++;
    pthread_mutex_init(&new_user->acquired, NULL);

    dbg("add: new user uid=%d", new_user->data->uid);

    /* add new users in users.txt (TODO) */

    return 0;
}


/*
 * Exported functions
 */

int robin_user_acquire_data(const char *email, const char *psw, robin_user_data_t **data)
{
    int ret = 0;

    dbg("acquire_data: email=%s psw=%s", email, psw);

    pthread_mutex_lock(&users_mutex);

    /* default return value: invalid email and password */
    ret = 2;
    for (int i = 0; i < users_len; i++) {
        if (strcmp(email, users[i].data->email) ||
                strcmp(psw, users[i].data->psw))
            continue;

        ret = pthread_mutex_trylock(&users[i].acquired);
        if (ret == 0) {
            *data = users[i].data;
            break;
        } else if (ret == EBUSY) {
            warn("user data already acquired by someone else");
            ret = 1;
            break;
        } else {
            err("failed to acquire the user data");
            ret = -1;
            break;
        }
    }

    dbg("acquire_data: ret=%d", ret);

    pthread_mutex_unlock(&users_mutex);
    return ret;
}

void robin_user_release_data(robin_user_data_t *data)
{
    pthread_mutex_lock(&users_mutex);
    pthread_mutex_unlock(&users[data->uid].acquired);
    pthread_mutex_unlock(&users_mutex);
}


int robin_user_add(const char *email, const char *psw)
{
    int ret;

    pthread_mutex_lock(&users_mutex);
    ret = robin_user_add_unsafe(email, psw);
    pthread_mutex_unlock(&users_mutex);

    return ret;
}
