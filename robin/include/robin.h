/*
 * robin.h
 *
 * Header file containing common includes and macros for Robin Server,
 * Robin API and Robin Client.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#ifndef ROBIN_H
#define ROBIN_H

/*
 * Common include
 */

#include <errno.h>
#include <string.h>

#include "robin_log.h"


/*
 * Robin Server release string
 */

#define ROBIN_VERSION_CORE        "0.7.1"
#define ROBIN_VERSION_CORE_STRING "v" ROBIN_VERSION_CORE

#define ROBIN_PRERELEASE        "dev"
#define ROBIN_PRERELEASE_STRING ROBIN_VERSION_CORE_STRING \
                                "-" ROBIN_PRERELEASE

#define ROBIN_RELEASE_STRING ROBIN_VERSION_CORE_STRING


/*
 * Robin API release string
 */

#define ROBIN_API_VERSION_CORE        "0.1.0"
#define ROBIN_API_VERSION_CORE_STRING "v" ROBIN_API_VERSION_CORE

#define ROBIN_API_PRERELEASE        "dev"
#define ROBIN_API_PRERELEASE_STRING ROBIN_API_VERSION_CORE_STRING \
                                    "-" ROBIN_API_PRERELEASE

#define ROBIN_API_RELEASE_STRING ROBIN_API_PRERELEASE_STRING


/*
 * Robin Client release string
 */

#define ROBIN_CLIENT_VERSION_CORE        "0.1.0"
#define ROBIN_CLIENT_VERSION_CORE_STRING "v" ROBIN_CLIENT_VERSION_CORE

#define ROBIN_CLIENT_PRERELEASE        "dev"
#define ROBIN_CLIENT_PRERELEASE_STRING ROBIN_CLIENT_VERSION_CORE_STRING \
                                       "-" ROBIN_CLIENT_PRERELEASE

#define ROBIN_CLIENT_RELEASE_STRING ROBIN_CLIENT_PRERELEASE_STRING


/*
 * Utility macros
 */

#define _STR(x) #x
#define STR(x) _STR(x)


/*
 * Common types
 */

typedef struct list {
    struct list *next;
    void *ptr;
} list_t;

typedef struct clist {
    struct clist *next;
    const void *ptr;
} clist_t;

typedef const struct cclist {
    const struct cclist *next;
    const void *ptr;
} cclist_t;

#endif /* ROBIN_H */
