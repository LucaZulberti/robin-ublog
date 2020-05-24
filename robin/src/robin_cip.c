/*
 * robin_cip.c
 *
 * Handles the in-memory database of the Robin Cips sent by the users and
 * keeps track of hashtags and timestamps.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#include <pthread.h>

#include "robin.h"
#include "robin_cip.h"


/*
 * Log shortcuts
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_CIP, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_CIP, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_CIP, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_CIP, fmt, ## args)


/*
 * Local types and macros
 */

static robin_cip_t *cips = NULL;
static robin_cip_t *last_cip = NULL;
static pthread_mutex_t cips_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Exported functions
 */

int robin_cip_add(const char *user, const char *msg)
{
    robin_cip_t *new_cip;
    char *hashtag, *ptr;
    size_t hashtag_len = 0;

    new_cip = malloc(sizeof(robin_cip_t));
    if (!new_cip) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    new_cip->ts = time(NULL);

    new_cip->user = malloc((strlen(user) + 1) * sizeof(char));
    if (!new_cip->user) {
        err("malloc: %s", strerror(errno));
        return -1;
    }
    strcpy(new_cip->user, user);

    new_cip->msg = malloc((strlen(msg) + 1) * sizeof(char));
    if (!new_cip->msg) {
        err("malloc: %s", strerror(errno));
        return -1;
    }
    strcpy(new_cip->msg, msg);

    new_cip->hashtags = NULL;
    new_cip->hashtags_num = 0;

    ptr = new_cip->msg;
    while (ptr) {
        hashtag = strchr(ptr, '#');
        if (!hashtag)
            break;

        /* only alphanumeric characters are supported in hashtags */
        ptr = ++hashtag;
        while(isalnum(*(ptr++)))
            hashtag_len++;

        /* discard alone # */
        if (hashtag_len == 0)
            continue;

        new_cip->hashtags = realloc(new_cip->hashtags,
            (new_cip->hashtags_num + 1) * sizeof(robin_hashtag_t));
        if (!new_cip->hashtags) {
            err("realloc: %s", strerror(errno));
            return -1;
        }

        new_cip->hashtags[new_cip->hashtags_num].tag = hashtag;
        new_cip->hashtags[new_cip->hashtags_num].len = hashtag_len;
        new_cip->hashtags_num++;
    }

    new_cip->next = NULL;

    /* actually add the cip to the system */
    pthread_mutex_lock(&cips_mutex);

    new_cip->prev = last_cip;

    if (last_cip)
        last_cip->next = new_cip;
    else
        cips = new_cip;

    last_cip = new_cip;

    pthread_mutex_unlock(&cips_mutex);

    return 0;
}

int robin_cip_get_since(time_t ts, const robin_cip_t **cips, unsigned int *nums)
{
    robin_cip_t *ptr, *first_since = NULL;
    unsigned int n;

    pthread_mutex_lock(&cips_mutex);

    ptr = last_cip;
    n = 0;
    while (ptr && ptr->ts > ts) {
        first_since = ptr;
        ptr = ptr->prev;
        n++;
    }

    pthread_mutex_unlock(&cips_mutex);

    *cips = first_since;
    *nums = n;

    return 0;
}

void robin_cip_free_all(void)
{
    robin_cip_t *cip, *tmp;

    pthread_mutex_lock(&cips_mutex);

    cip = cips;
    while (cip) {
        dbg("cip_free: user=%p", cip->user);
        free(cip->user);

        dbg("cip_free: msg=%p", cip->msg);
        free(cip->msg);

        dbg("cip_free: hastags=%p", cip->hashtags);
        free(cip->hashtags);

        tmp = cip;
        cip = cip->next;

        dbg("cip_free: cip=%p", tmp);
        free(tmp);
    }

    cips = NULL;
    last_cip = NULL;

    pthread_mutex_unlock(&cips_mutex);
}

