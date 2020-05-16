/*
 * robin_conn.h
 *
 * Header file for Robin Connection layer
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_CONN_H
#define ROBIN_CONN_H

/**
 * @brief Manage the connection with the client
 *
 * @param id Connection ID (Robin Thread ID)
 * @param fd Socket file descriptor
 */
void robin_conn_manage(int id, int fd);

/**
 * @brief Terminate the Robin Connection gracefully
 *
 * @param id Connection ID (Robin Thread ID)
 */
void robin_conn_terminate(int id);

#endif /* ROBIN_CONN_H */
