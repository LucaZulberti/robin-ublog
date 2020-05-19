/*
 * robin_thread.c
 *
 * The Robin Thread Pool handles the incoming connections.
 *
 * N threads are spawned at start-up with N as compile-time constant.
 * N is also the max number of simultaneous connections.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>

#include "robin.h"
#include "robin_conn.h"
#include "robin_thread.h"


/*
 * Log shortcuts
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_POOL, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_POOL, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_POOL, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_POOL, fmt, ## args)


/*
 * Robin Thread types and data
 */

#define ROBIN_THREAD_POOL_RT_NUM 4

typedef enum rt_state {
    RT_FREE = 0,
    RT_BUSY
} rt_state_t;

typedef struct robin_thread {
    pthread_t thread;  /* phtread fd */
    unsigned int id;   /* thread id */
    int fd;            /* associated socket file descriptor */

    /* Robin Thread state */
    rt_state_t      state;
    pthread_cond_t  state_cond;
    pthread_mutex_t state_mutex;

    struct robin_thread *next; /* next available Robin Thread if not busy */
} robin_thread_t;

static robin_thread_t *rt_pool;

static robin_thread_t *rt_free_list = NULL;
static pthread_cond_t  rt_free_list_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t rt_free_list_mutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * Local functions
 */

static int rt_init(robin_thread_t *rt, int id)
{
    rt->id = id;
    rt->fd = -1;

    /* initialize RT state to free */
    rt->state = RT_FREE;
    if (pthread_mutex_init(&rt->state_mutex, NULL)) {
        err("%s", strerror(errno));
        return -1;
    }
    if (pthread_cond_init(&rt->state_cond, NULL)) {
        err("%s", strerror(errno));
        return -1;
    }

    rt->next = NULL;

    return 0;
}

static void rt_state_set(robin_thread_t *rt, rt_state_t state)
{
    pthread_mutex_lock(&rt->state_mutex);

    rt->state = state;

    pthread_cond_signal(&rt->state_cond);
    pthread_mutex_unlock(&rt->state_mutex);
}

static void rt_cleanup(void *arg)
{
    robin_thread_t *me = (robin_thread_t *) arg;

    robin_conn_terminate(me->id);
}

static inline void rt_free_list_push_unsafe(robin_thread_t *rt)
{
    rt->next = rt_free_list;
    rt_free_list = rt;
}

static void rt_free_list_push(robin_thread_t *rt)
{
    pthread_mutex_lock(&rt_free_list_mutex);

    rt_free_list_push_unsafe(rt);

    pthread_cond_signal(&rt_free_list_cond);
    pthread_mutex_unlock(&rt_free_list_mutex);
}

static robin_thread_t *rt_free_list_pop(void)
{
    robin_thread_t *popped;

    pthread_mutex_lock(&rt_free_list_mutex);
    while (rt_free_list == NULL)
        pthread_cond_wait(&rt_free_list_cond, &rt_free_list_mutex);

    /* pop the Robin Thread from free_list */
    popped = rt_free_list;
    rt_free_list = popped->next;
    pthread_mutex_unlock(&rt_free_list_mutex);

    return popped;
}

static void *rt_loop(void *ctx)
{
    robin_thread_t *me = (robin_thread_t *) ctx;
    const int rt_log_id = ROBIN_LOG_ID_RT_BASE + me->id;

    /* setup cleanup function */
    pthread_cleanup_push(rt_cleanup, me);

    /* Robin Thread loop */
    while (1) {
        pthread_mutex_lock(&me->state_mutex);
        switch (me->state) {
            case RT_FREE:
                robin_log_info(rt_log_id, "ready", me->id);
                pthread_cond_wait(&me->state_cond, &me->state_mutex);
                break;

            case RT_BUSY:
                robin_log_info(rt_log_id, "serving fd=%d", me->fd);

                /* handle requests from client until disconnected */
                robin_conn_manage(me->id, me->fd);

                /* re-initialize this RT's data */
                me->fd = -1;

                /* push this RT in the free list */
                me->state = RT_FREE;
                rt_free_list_push(me);
                break;
        }
        pthread_mutex_unlock(&me->state_mutex);
    }

    /* do not execute clean-up, this should not be reached */
    pthread_cleanup_pop(0);

    pthread_exit(NULL);
}


/*
 * Exported functions
 */

int robin_thread_pool_init(void)
{
    int ret;

    rt_pool = malloc(ROBIN_THREAD_POOL_RT_NUM
                     * sizeof(robin_thread_t));
    if (!rt_pool) {
        err("%s", strerror(errno));
        return -1;
    }

    info("spawning %d Robin Threads...", ROBIN_THREAD_POOL_RT_NUM);

    for (int i = 0; i < ROBIN_THREAD_POOL_RT_NUM; i++) {
        /* initialize the Robin Thread data */
        if (rt_init(&rt_pool[i], i) < 0) {
            err("failed to initialize the Robin Thread #%d", i);
            exit(EXIT_FAILURE);
        }

        /* add it to the free list */
        rt_free_list_push_unsafe(&rt_pool[i]);

        /* spawn the thread */
        ret = pthread_create(&rt_pool[i].thread, NULL, rt_loop, &rt_pool[i]);
        if (ret) {
            err("%s", strerror(ret));
            return -1;
        }
    }

    return 0;
}

void robin_thread_pool_dispatch(int fd)
{
    robin_thread_t *rt;

    /* take a free Robin Thread (wait for an available RT) */
    rt = rt_free_list_pop();
    info("thread %d selected", rt->id);

    /* associate socket with the Robin Thread */
    rt->fd = fd;

    /* wake up the Robin Thread */
    rt_state_set(rt, RT_BUSY);
}

void robin_thread_pool_free(void)
{
    for (int i = 0; i < ROBIN_THREAD_POOL_RT_NUM; i++) {
        dbg("cancel: tid=%d, t=%p", i, rt_pool[i].thread);
        pthread_cancel(rt_pool[i].thread);
        pthread_join(rt_pool[i].thread, NULL);
    }

    dbg("free: rt_pool=%p", rt_pool);
    free(rt_pool);
}
