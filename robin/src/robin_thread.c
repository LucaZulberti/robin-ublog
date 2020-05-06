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
 * Robin Thread
 */

#define ROBIN_SERVER_THREAD_NUMBER 4

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

    /* Robin shared thread data */
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

    /* Robin thread allocation loop */
    while (1) switch (me->data.state) {
        case RT_FREE:
            pthread_cond_wait(&me->data_cond, &me->data_mutex);
            break;

        case RT_BUSY:
            pthread_mutex_unlock(&me->data_mutex);

            /* Handle requests from client until disconnected */
            /* TODO */

            pthread_mutex_lock(&me->data_mutex);
            robin_thread_data_init(&me->data);
            pthread_mutex_unlock(&me->data_mutex);
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

    /*
     * Semaphore initialization
     */

    ret = sem_init(&robin_thread_free, 0, ROBIN_SERVER_THREAD_NUMBER);
    if (ret) {
        robin_log_err("%s", strerror(errno));
        return -1;
    }


    /*
     * Thread spawning
     */

    robin_thread_pool = malloc(ROBIN_SERVER_THREAD_NUMBER
                               * sizeof(robin_thread_t));
    if (!robin_thread_pool) {
        robin_log_err("%s", strerror(errno));
        return -1;
    }

    for (int i = 0; i < ROBIN_SERVER_THREAD_NUMBER; i++) {
        robin_thread_init(&robin_thread_pool[i], i);
        ret = pthread_create(&robin_thread_pool[i].thread, NULL,
                             robin_thread_function, &robin_thread_pool[i]);
        if (ret) {
            robin_log_err("%s", strerror(ret));
            return -1;
        }
    }

    return 0;
}

void robin_thread_allocate(int fd)
{
    robin_thread_t *rt;

    /* wait for a free thread */
    sem_wait(&robin_thread_free);

    for (int i = 0; i < ROBIN_SERVER_THREAD_NUMBER; i++) {
        rt = &robin_thread_pool[i];

        pthread_mutex_lock(&rt->data_mutex);
        if (rt->data.state == RT_FREE) {
            /* setup robin thread data */
            rt->data.state = RT_BUSY;
            rt->data.fd = fd;

            pthread_mutex_unlock(&rt->data_mutex);
            pthread_cond_signal(&rt->data_cond); /* wake up thread */
            break;
        } else {
            pthread_mutex_unlock(&rt->data_mutex);
        }
    }
}
