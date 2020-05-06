/*
 * robin.h
 *
 * Header file containing common types, macro and functions definitions for
 * Robin Server.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_SERVER_H
#define ROBIN_SERVER_H

#include <pthread.h>

#define ROBIN_SERVER_THREAD_NUMBER 4

typedef struct robin_thread {
    pthread_t thread;  /* phtread fd */
    unsigned int id;   /* thread id */
} robin_thread_t;

#endif /* ROBIN_SERVER_H */
