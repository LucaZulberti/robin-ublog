/*
 * robin_manager.h
 *
 * Header file containing public interface for Robin Manager.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_MANAGER_H
#define ROBIN_MANAGER_H

#include "robin_thread.h"

/**
 * @brief Manage the connection with the client
 *
 * @param rt Robin Thread handling the connection
 */
void robin_manage_connection(robin_thread_t *rt);

#endif /* ROBIN_MANAGER_H */
