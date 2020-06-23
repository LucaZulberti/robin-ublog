/*
 * robin_cli.h
 *
 * Header file for Robin CLI layer
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_CLI_H
#define ROBIN_CLI_H

/**
 * @brief Manage the UI with the user on command line
 *
 * @param fd file descriptor of Robin Server connection
 */
void robin_cli_manage(int fd);

/**
 * @brief Terminate the Robin CLI gracefully
 */
void robin_cli_terminate(void);

#endif /* ROBIN_CLI_H */
