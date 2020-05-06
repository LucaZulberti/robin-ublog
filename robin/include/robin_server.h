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

/*
 * Create and spawn all Robin threads
 */
int robin_thread_pool_init(void);

#endif /* ROBIN_SERVER_H */
