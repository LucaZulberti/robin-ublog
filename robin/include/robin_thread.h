/*
 * robin_thread.h
 *
 * Header file containing public interface for Robin Thread Pool utilization.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_THREAD_H
#define ROBIN_THREAD_H

/*
 * Create and spawn all Robin threads
 */
int robin_thread_pool_init(void);

#endif /* ROBIN_THREAD_H */
