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

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

#include "robin.h"
#include "robin_manager.h"
#include "robin_thread.h"


/*
 * Robin Thread types and data
 */

#define ROBIN_THREAD_POOL_RT_NUM 4

typedef struct robin_thread {
    pthread_t thread;  /* phtread fd */
    unsigned int id;   /* thread id */

    sem_t busy;                /* semaphore for non-active wait when free */
    struct robin_thread *next; /* next available Robin Thread if not busy */

    /* Robin Thread data */
    int fd;
} robin_thread_t;

static const int log_id = ROBIN_LOG_ID_POOL;
static robin_thread_t *rt_pool;
static robin_thread_t *rt_free_list = NULL;
static pthread_cond_t rt_free_list_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t rt_free_list_mutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * Local functions
 */

static int rt_init(robin_thread_t *rt, int id)
{
    rt->id = id;

    /* initially free */
    if (sem_init(&rt->busy, 0, 0)) {
        robin_log_err(log_id, "%s", strerror(errno));
        return -1;
    }
    rt->next = NULL;

    rt->fd = -1;

    return 0;
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

    /* Robin Thread loop */
    while (1) {
        robin_log_info(rt_log_id, "ready", me->id);
        sem_wait(&me->busy);

        robin_log_info(rt_log_id, "serving fd=%d", me->id, me->fd);

        /* handle requests from client until disconnected */
        robin_manage_connection(rt_log_id, me->fd);

        /* re-initialize this RT's data */
        me->fd = -1;

        /* push this RT in the free list */
        rt_free_list_push(me);
    }

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
        robin_log_err(log_id, "%s", strerror(errno));
        return -1;
    }

    robin_log_info(log_id, "spawning %d Robin Threads...",
                   ROBIN_THREAD_POOL_RT_NUM);

    for (int i = 0; i < ROBIN_THREAD_POOL_RT_NUM; i++) {
        /* initialize the Robin Thread data */
        if (rt_init(&rt_pool[i], i) < 0) {
            robin_log_err(log_id, "failed to initialize the Robin Thread #%d",
                          i);
            exit(EXIT_FAILURE);
        }

        /* add it to the free list */
        rt_free_list_push_unsafe(&rt_pool[i]);

        /* spawn the thread */
        ret = pthread_create(&rt_pool[i].thread, NULL, rt_loop, &rt_pool[i]);
        if (ret) {
            robin_log_err(log_id, "%s", strerror(ret));
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
    robin_log_info(log_id, "thread %d selected", rt->id);

    /* setup data on the Robin Thread */
    rt->fd = fd;

    /* wake up the Robin Thread */
    sem_post(&rt->busy);
}
