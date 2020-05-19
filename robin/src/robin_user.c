/*
 * robin_user.c
 *
 * This file contains the functions to handle user information stored in
 * memory and on the file system.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include "robin.h"
#include "robin_user.h"

#include <stdlib.h>

#include <pthread.h>


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

#define ROBIN_USER_EMAIL_LEN 64
#define ROBIN_USER_PSW_LEN   64

typedef struct robin_user_data {
    /* Login information */
    char email[ROBIN_USER_EMAIL_LEN];
    char psw[ROBIN_USER_PSW_LEN];  /* hashed password */

    /* Social data */
    clist_t *following;
    size_t   following_len;
} robin_user_data_t;

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
    int i, uid;

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

    users = realloc(users, (users_len + 1) * sizeof(robin_user_t));
    if (!users) {
        err("realloc: %s", strerror(errno));
        return -1;
    }
    uid = users_len++;

    users[uid].data = malloc(sizeof(robin_user_data_t));
    if (!users[uid].data) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    strcpy(users[uid].data->email, email);
    strcpy(users[uid].data->psw, psw);
    users[uid].data->following = NULL;
    users[uid].data->following_len = 0;
    dbg("add: data allocated and initialized");

    pthread_mutex_init(&users[uid].acquired, NULL);

    dbg("add: new user uid=%d", uid);

    /* add new users in users.txt (TODO) */

    return 0;
}

static int robin_user_is_acquired(robin_user_t *user)
{
    int ret;

    ret = pthread_mutex_trylock(&user->acquired);
    dbg("is_acquired: trylock: ret=%d", ret);
    if (ret == 0) {
        pthread_mutex_unlock(&user->acquired);
        return 0;
    } else if (ret == EBUSY) {
        return 1;
    } else {
        err("pthread_mutex_trylock: ret=%d", ret);
        return -1;
    }
}

static void robin_user_data_free_unsafe(robin_user_data_t *data)
{
    clist_t *cur, *next;

    if (data) {
        if (data->following) {
            cur = data->following;

            while (cur != NULL) {
                next = cur->next;
                dbg("user_data_free: following=%p", cur);
                free(cur);
                cur = next;
            }
        }

        dbg("user_data_free: data=%p", data);
        free(data);
    }
}


/*
 * Exported functions
 */

int robin_user_acquire(const char *email, const char *psw, int *uid)
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
            *uid = i;
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

void robin_user_release(int uid)
{
    pthread_mutex_lock(&users_mutex);

    if (robin_user_is_acquired(&users[uid]) > 0)
        pthread_mutex_unlock(&users[uid].acquired);

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

const char *robin_user_email_get(int uid)
{
    const char *ret = NULL;

    pthread_mutex_lock(&users_mutex);

    if (robin_user_is_acquired(&users[uid]))
        ret = users[uid].data->email;

    pthread_mutex_unlock(&users_mutex);
    return ret;
}

int robin_user_following_get(int uid, char ***following, size_t *len)
{
    robin_user_data_t *data;
    clist_t *tmp;
    char **following_vec;
    int ret = 0;

    pthread_mutex_lock(&users_mutex);
    if (robin_user_is_acquired(&users[uid]))
        data = users[uid].data;
    else
        ret = -1;
    pthread_mutex_unlock(&users_mutex);

    if (ret)
        return ret;

    following_vec = malloc(data->following_len * sizeof(char *));
    if (!following_vec) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    tmp = data->following;
    for (int i = 0; i < data->following_len; i++) {
        following_vec[i] = ((robin_user_data_t *) tmp->ptr)->email;
        tmp = tmp->next;
    }

    *following = following_vec;
    *len = data->following_len;

    return 0;
}

int robin_user_follow(int uid, const char *email)
{
    robin_user_data_t *me, *found = NULL;
    clist_t *el, *new_follow;

    /* exclusive access for retrieve data pointers */
    pthread_mutex_lock(&users_mutex);

    if (!robin_user_is_acquired(&users[uid])) {
        err("follow: user %d (%s) is not acquired", uid,
            users[uid].data->email);
        return -1;
    }

    me = users[uid].data;

    for (int i = 0; i < users_len; i++) {
        /* an user cannot follow himself */
        if (i == uid)
            continue;

        if (!strcmp(email, users[i].data->email)) {
            found = users[i].data;
            break;
        }
    }

    pthread_mutex_unlock(&users_mutex);

    /*
     * Users cannot be deleted at run time, exclusive access
     * is not needed anymore.
     */

    if (!found) {
        warn("follow: user %s does not exist", email);
        return 1;
    }

    /* search for already followed user */
    el = me->following;
    while (el != NULL) {
        const char *el_mail = (const char *) el->ptr;

        if (!strcmp(el_mail, email))
            break;

        el = el->next;
    }

    if (el) {
        warn("follow: user %s is already followed", found->email);
        return 2;
    }

    new_follow = malloc(sizeof(clist_t));
    if (!new_follow) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    new_follow->ptr = found;
    new_follow->next = me->following;
    me->following = new_follow;
    me->following_len++;

    return 0;
}


int robin_user_unfollow(int uid, const char *email)
{
    robin_user_data_t *me;
    clist_t *el, *prev;

    /* exclusive access for retrieve data pointers */
    pthread_mutex_lock(&users_mutex);

    if (!robin_user_is_acquired(&users[uid])) {
        err("follow: user %d (%s) is not acquired", uid,
            users[uid].data->email);
        return -1;
    }

    me = users[uid].data;

    pthread_mutex_unlock(&users_mutex);

    /*
     * Users cannot be deleted at run time, exclusive access
     * is not needed anymore.
     */

    /* search for followed user */
    prev = NULL;
    el = me->following;
    while (el != NULL) {
        const char *el_mail = ((robin_user_data_t *) el->ptr)->email;

        if (!strcmp(el_mail, email))
            break;

        prev = el;
        el = el->next;
    }

    if (!el) {
        warn("follow: user %s is not followed", email);
        return 1;
    }

    if (prev)
        prev->next = el->next;
    else
        me->following = el->next;

    me->following_len--;

    free(el);

    return 0;
}

void robin_user_free_all(void)
{
    pthread_mutex_lock(&users_mutex);

    for (int i = 0; i < users_len; i++) {
        /* skip acquired resources */
        if (robin_user_is_acquired(&users[i]))
            continue;

        pthread_mutex_lock(&users[i].acquired);
        robin_user_data_free_unsafe(users[i].data);
        pthread_mutex_unlock(&users[i].acquired);
    }

    dbg("user_free_all: users=%p", users);
    free(users);

    pthread_mutex_unlock(&users_mutex);
}
