/*
 * robin_thread.h
 *
 * Header file containing public interface for Robin Thread Pool utilization.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_THREAD_H
#define ROBIN_THREAD_H

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
