/*
 * alloc_safe.h
 *
 * Header file containing the pthread_cancel-safe allocation functions.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ALLOC_SAFE_H
#define ALLOC_SAFE_H

#include <stdlib.h>
#include <pthread.h>

static inline void malloc_safe(void **dest, size_t size)
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    *dest = malloc(size);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
}

static inline void calloc_safe(void **dest, size_t n, size_t size)
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    *dest = calloc(n, size);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
}

static inline void realloc_safe(void **dest, void *ptr, size_t size)
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    *dest = realloc(ptr, size);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
}

static inline void free_safe(void **dest, void *new_val)
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    free(*dest);
    *dest = new_val;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
}

#endif /* ALLOC_SAFE_H */
