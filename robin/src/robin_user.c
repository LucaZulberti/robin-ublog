/*
 * robin_user.c
 *
 * This file contains the functions to handle user information stored in
 * memory and on the file system.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <stdio.h>
#include <stdlib.h>

#include <crypt.h>
#include <pthread.h>

#include "robin.h"
#include "robin_user.h"
#include "lib/password.h"

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
    char email[ROBIN_USER_EMAIL_LEN + 1];
    char psw[ROBIN_USER_PSW_LEN + 1];  /* hashed password */

    /* Social data */
    clist_t *following;
    size_t   following_len;
    clist_t *followers;
    size_t   followers_len;
    /* Synchronization needed because other threads can set the followers */
    pthread_mutex_t followers_mutex;
} robin_user_data_t;

typedef struct robin_user {
    robin_user_data_t *data; /* user data */
    pthread_mutex_t acquired; /* exclusive access to user data */
} robin_user_t;


/*
 * Local data
 */

static char *users_file = NULL;
static robin_user_t *users = NULL;
static int users_len = 0;
static pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * Local functions
 */

static int robin_user_add_unsafe(const char * email, const char * psw)
{
    int i, uid;
    FILE *fp;
    size_t email_len, psw_len;

    dbg("add: email=%s psw=%s", email, psw);

    email_len = strlen(email);
    if (email_len > ROBIN_USER_EMAIL_LEN - 1) {
        warn("add: email is longer than " STR(ROBIN_USER_EMAIL_LEN)
             " characters");
        return 1;
    }

    psw_len = strlen(psw);
    if (psw_len > ROBIN_USER_PSW_LEN - 1) {
        warn("add: password is longer than " STR(ROBIN_USER_PSW_LEN)
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
    users[uid].data->followers = NULL;
    users[uid].data->followers_len = 0;
    if (pthread_mutex_init(&users[uid].data->followers_mutex, NULL) < 0) {
        err("pthread_mutex_init: %s", strerror(errno));
        return -1;
    }
    dbg("add: data allocated and initialized");

    pthread_mutex_init(&users[uid].acquired, NULL);

    dbg("add: new user uid=%d", uid);

    /* do not add user on file system if file pointer is not initialized */
    if (!users_file)
        return 0;

    fp = fopen(users_file, "a+");
    if (!fp) {
        err("fopen: %s", strerror(errno));
        return -1;
    }
    fprintf(fp, "%s:%s\n", email, psw);
    fclose(fp);

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
                dbg("data_free: following=%p", cur);
                free(cur);
                cur = next;
            }
        }

        if (data->followers) {
            cur = data->followers;

            while (cur != NULL) {
                next = cur->next;
                dbg("data_free: followers=%p", cur);
                free(cur);
                cur = next;
            }
        }

        dbg("data_free: data=%p", data);
        free(data);
    }
}


/*
 * Exported functions
 */

int robin_users_load(const char *filename)
{
    FILE *fp;
    char *buf = NULL, *ptr;
    size_t filename_len, buf_len = 0;
    ssize_t nread;

    dbg("load: open file %s", filename);

    fp = fopen(filename, "a+");
    if (!fp) {
        err("fopen: %s", strerror(errno));
        return -1;
    }

    while ((nread = getline(&buf, &buf_len, fp)) != -1) {
        /* remove new line from buffer if present */
        ptr = strchr(buf, '\n');
        if (ptr)
            *ptr = '\0';

        /* separate email and password */
        ptr = strchr(buf, ':');
        if (!ptr) {
            err("load: invalid format of user file");
            return -1;
        }
        *(ptr++) = '\0';

        /* actually add the user */
        if (robin_user_add_unsafe(buf, ptr) < 0) {
            err("load: failed to add the user %s to the system", buf);
            return -1;
        }
    }

    free(buf);
    fclose(fp);

    dbg("load: all users have been register into the system");

    /* save filename for successive user registrations */
    filename_len = strlen(filename) + 1;
    users_file = malloc(filename_len * sizeof(char));
    if (!users_file) {
        err("malloc: %s", strerror(errno));
        return -1;
    }
    strcpy(users_file, filename);

    return 0;
}

int robin_user_acquire(const char *email, const char *psw, int *uid)
{
    char psw_hashed[512];
    int ret;

    dbg("acquire_data: email=%s psw=%s", email, psw);

    pthread_mutex_lock(&users_mutex);

    /* default return value: invalid email */
    ret = 2;
    for (int i = 0; i < users_len; i++) {
        if (strcmp(email, users[i].data->email))
            continue;

        ret = password_hash(psw_hashed, psw, users[i].data->psw);
        if (ret < 0) {
            err("acquire: failed to hash the password");
            return -1;
        }

        if (strcmp(psw_hashed, users[i].data->psw)) {
            /* invalid password */
            ret = 3;
            break;
        }

        ret = pthread_mutex_trylock(&users[i].acquired);
        if (ret == 0) {
            *uid = i;
            break;
        } else if (ret == EBUSY) {
            warn("acquire: user data already acquired by someone else");
            ret = 1;
            break;
        } else {
            err("acquire: failed to acquire the user data");
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
    char psw_hashed[512];
    int ret;

    ret = password_hash(psw_hashed, psw, NULL);
    if (ret < 0) {
        err("add: could not hash the password");
        return -1;
    }

    pthread_mutex_lock(&users_mutex);
    ret = robin_user_add_unsafe(email, psw_hashed);
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

int robin_user_followers_get(int uid, char ***followers, size_t *len)
{
    robin_user_data_t *data;
    clist_t *tmp;
    char **followers_vec;
    int ret = 0;

    pthread_mutex_lock(&users_mutex);

    if (robin_user_is_acquired(&users[uid]))
        data = users[uid].data;
    else
        ret = -1;

    pthread_mutex_unlock(&users_mutex);

    if (ret)
        return ret;

    pthread_mutex_lock(&data->followers_mutex);

    followers_vec = malloc(data->followers_len * sizeof(char *));
    if (!followers_vec) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    tmp = data->followers;
    for (int i = 0; i < data->followers_len; i++) {
        char *email = ((robin_user_data_t *) tmp->ptr)->email;
        dbg("followers_get: vec[%d] = %s", i, email);
        followers_vec[i] = email;
        tmp = tmp->next;
    }

    pthread_mutex_unlock(&data->followers_mutex);

    *followers = followers_vec;
    *len = data->followers_len;

    return ret;
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
        const char *el_email = ((robin_user_data_t *) el->ptr)->email;

        if (!strcmp(el_email, email))
            break;

        el = el->next;
    }

    if (el) {
        warn("follow: user %s is already followed", found->email);
        return 2;
    }

    /* add following user */
    new_follow = malloc(sizeof(clist_t));
    if (!new_follow) {
        err("malloc: %s", strerror(errno));
        return -1;
    }
    new_follow->ptr = found;
    new_follow->next = me->following;
    me->following = new_follow;
    me->following_len++;
    dbg("follow: following=%s, len=%d", found->email, me->following_len);

    /* add me as follower in following user */
    new_follow = malloc(sizeof(clist_t));
    if (!new_follow) {
        err("malloc: %s", strerror(errno));
        return -1;
    }
    new_follow->ptr = me;
    pthread_mutex_lock(&found->followers_mutex);
    new_follow->next = found->followers;
    found->followers = new_follow;
    found->followers_len++;
    dbg("follow: follower=%s, len=%d", me->email, found->followers_len);
    pthread_mutex_unlock(&found->followers_mutex);

    return 0;
}

int robin_user_unfollow(int uid, const char *email)
{
    robin_user_data_t *me, *unfollowed;
    clist_t *el, *prev, *tmp;

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
    tmp = me->following;
    while (tmp != NULL) {
        const char *el_mail = ((robin_user_data_t *) tmp->ptr)->email;

        if (!strcmp(el_mail, email))
            break;

        prev = tmp;
        tmp = tmp->next;
    }

    if (!tmp) {
        warn("follow: user %s is not followed", email);
        return 1;
    }

    if (prev)
        prev->next = tmp->next;
    else
        me->following = tmp->next;

    me->following_len--;
    el = tmp;
    unfollowed = (robin_user_data_t *) el->ptr;

    pthread_mutex_lock(&unfollowed->followers_mutex);

    prev = NULL;
    tmp = unfollowed->followers;
    while (tmp) {
        const char *el_email = ((robin_user_data_t *) tmp->ptr)->email;
        if (!strcmp(el_email, me->email))
            break;

        prev = tmp;
        tmp = tmp->next;
    }

    if (!tmp) {
        warn("follow: user %s is not a follower of user %s",
             email, unfollowed->email);
        return 1;
    }

    if (prev)
        prev->next = tmp->next;
    else
        unfollowed->followers = tmp->next;

    unfollowed->followers_len--;

    pthread_mutex_unlock(&unfollowed->followers_mutex);

    free(el);

    return 0;
}

void robin_user_free_all(void)
{
    pthread_mutex_lock(&users_mutex);

    if (users) {
        for (int i = 0; i < users_len; i++) {
            /* skip acquired resources */
            if (robin_user_is_acquired(&users[i]))
                continue;

            pthread_mutex_lock(&users[i].acquired);
            robin_user_data_free_unsafe(users[i].data);
            pthread_mutex_unlock(&users[i].acquired);
        }

        dbg("free_all: users=%p", users);
        free(users);
    }

    if (users_file) {
        dbg("free_all: users_file=%p", users_file);
        free(users_file);
    }

    pthread_mutex_unlock(&users_mutex);
}
