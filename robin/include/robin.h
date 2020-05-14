/*
 * robin.h
 *
 * Header file containing common includes and macros for Robin Server and
 * Robin API (internals).
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_H
#define ROBIN_H

/*
 * Common include
 */

#include "robin_log.h"


/*
 * Robin release string
 */

#define ROBIN_VERSION_CORE          "0.4.1"
#define ROBIN_VERSION_CORE_STRING   "v" ROBIN_VERSION_CORE

#define ROBIN_PRERELEASE            "dev"
#define ROBIN_PRERELEASE_STRING     ROBIN_VERSION_CORE_STRING \
                                    "-" ROBIN_PRERELEASE

#define ROBIN_RELEASE_STRING        ROBIN_PRERELEASE_STRING


/*
 * Utility macros
 */

#define _STR(x) #x
#define STR(x) _STR(x)

#endif /* ROBIN_H */
