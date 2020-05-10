/*
 * robin_thread.h
 *
 * Header file containing public interface for Robin Thread Pool utilization.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_THREAD_H
#define ROBIN_THREAD_H

#include <stddef.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct robin_thread {
    pthread_t thread;  /* phtread fd */
    unsigned int id;   /* thread id */

    sem_t busy;                /* semaphore for non-active wait when free */
    struct robin_thread *next; /* next available Robin Thread if not busy */

    /* Robin Thread data */
    int fd;
    char *buf;
    size_t len;
} robin_thread_t;

/**
 * @brief Create and spawn all Robin threads in pool.
 *
 * @return int 0 on success, -1 on failure.
 */
int robin_thread_pool_init(void);

/**
 * @brief Dispatch a connection to a free Robin Thread in the pool.
 *
 * The function will block if there are no threads available.
 *
 * @param fd socket file descriptor of the accepted connection
 */
void robin_thread_pool_dispatch(int fd);

#endif /* ROBIN_THREAD_H */
