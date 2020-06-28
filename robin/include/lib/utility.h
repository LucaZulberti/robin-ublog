/*
 * utility.h
 *
 * Header file for utility functions
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef UTILITY_H
#define UTILITY_H

/**
 * @brief build (argc, argv) parameters from line
 *
 * @param src  the line to parse
 * @param argc number of argument found
 * @param argv the arguments
 * @return int 0 on success, -1 on error
 */
int argv_parse(char *src, int *argc, char ***argv);

#endif  /* UTILITY_H */
