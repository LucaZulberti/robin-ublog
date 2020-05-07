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
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#include "robin.h"
#include "robin_thread.h"


/*
 * Robin Thread Pool types and data
 */

#define ROBIN_THREAD_POOL_RT_NUM 4

typedef enum robin_thread_state {
    RT_FREE = 0,
    RT_BUSY
} robin_thread_state_t;

typedef struct robin_thread_data {
    robin_thread_state_t state; /* current thread state */
    int fd;                     /* connection file descriptor */
} robin_thread_data_t;

typedef struct robin_thread {
    pthread_t thread;  /* phtread fd */
    unsigned int id;   /* thread id */

    /*
     * Robin Thread data
     *
     * Synchronization is necessary for possible race-conditions with the
     * dispatcher function that is searching for RT_FREE.
     */
    robin_thread_data_t data;
    pthread_mutex_t data_mutex;
    pthread_cond_t data_cond;
} robin_thread_t;

static robin_thread_t *robin_thread_pool;
static sem_t robin_thread_free;


/*
 * Local functions
 */

static void robin_thread_data_init(robin_thread_data_t *data)
{
    data->state = RT_FREE;
    data->fd = -1;
}

static void *robin_thread_function(void *ctx)
{
    robin_thread_t *me = (robin_thread_t *) ctx;

    pthread_mutex_lock(&me->data_mutex);

    /* Robin Thread allocation loop */
    while (1) switch (me->data.state) {
        case RT_FREE:
            robin_log_info("thread %d: ready", me->id);
            pthread_cond_wait(&me->data_cond, &me->data_mutex);
            break;

        case RT_BUSY:
            pthread_mutex_unlock(&me->data_mutex);
            robin_log_info("thread %d: serving fd=%d", me->id, me->data.fd);

            /* handle requests from client until disconnected */
            /* TODO */

            /*
             * Re-initialize the Robin Thread data
             *
             * The lock here is necessary to avoid race-conditions with
             * robin_thread_pool_dispatch() when it is searching for RT_FREE.
             */
            pthread_mutex_lock(&me->data_mutex);
            robin_thread_data_init(&me->data);
            pthread_mutex_unlock(&me->data_mutex);

            /* notify the dispatcher that a new thread is free */
            sem_post(&robin_thread_free);
            break;
    }

    pthread_exit(NULL);
}

static void robin_thread_init(robin_thread_t *rt, int id)
{
    rt->id = id;

    robin_thread_data_init(&rt->data);
    pthread_mutex_init(&rt->data_mutex, NULL);
    pthread_cond_init(&rt->data_cond, NULL);
}


/*
 * Exported functions
 */

int robin_thread_pool_init(void)
{
    int ret;

    /* semaphore initialization: all Robin Threads are free */
    ret = sem_init(&robin_thread_free, 0, ROBIN_THREAD_POOL_RT_NUM);
    if (ret) {
        robin_log_err("%s", strerror(errno));
        return -1;
    }


    /*
     * Thread spawning
     */

    robin_thread_pool = malloc(ROBIN_THREAD_POOL_RT_NUM
                               * sizeof(robin_thread_t));
    if (!robin_thread_pool) {
        robin_log_err("%s", strerror(errno));
        return -1;
    }

    robin_log_info("main: spawning %d Robin Threads...", ROBIN_THREAD_POOL_RT_NUM);

    for (int i = 0; i < ROBIN_THREAD_POOL_RT_NUM; i++) {
        /* initialize the Robin Thread data */
        robin_thread_init(&robin_thread_pool[i], i);

        /* spawn the thread */
        ret = pthread_create(&robin_thread_pool[i].thread, NULL,
                             robin_thread_function, &robin_thread_pool[i]);
        if (ret) {
            robin_log_err("%s", strerror(ret));
            return -1;
        }
    }

    return 0;
}

void robin_thread_pool_dispatch(int fd)
{
    robin_thread_t *rt;

    /* wait for a free thread */
    sem_wait(&robin_thread_free);

    /* dispatcher */
    for (int i = 0; i < ROBIN_THREAD_POOL_RT_NUM; i++) {
        rt = &robin_thread_pool[i];

        pthread_mutex_lock(&rt->data_mutex);
        if (rt->data.state == RT_FREE) {
            /* setup robin thread data */
            rt->data.state = RT_BUSY;
            rt->data.fd = fd;
            pthread_mutex_unlock(&rt->data_mutex);

            /* wake up the chosen thread */
            robin_log_info("main: thread %d selected", i);
            pthread_cond_signal(&rt->data_cond);
            break;
        } else {
            pthread_mutex_unlock(&rt->data_mutex);
        }
    }
}
