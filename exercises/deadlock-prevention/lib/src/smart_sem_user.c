///                MentOS, The Mentoring Operating system project
/// @file smart_sem_user.c
/// @brief Semaphore user-side implementation source code.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "smart_sem_user.h"

#include <errno.h>
#include <syslog.h>
#include <system/syscall_types.h>

int sem_create()
{
    int retval = 0;

    /* ... */

    syslog(LOG_INFO, "sem_create() -> %d\n", retval);

    return retval;
}

int sem_destroy(int id)
{
    int retval = 0;

    syslog(LOG_INFO, "sem_destroy(%d)\n", id);

    /* ... */

    return retval;
}

int sem_init(int id)
{
    int retval = 0;

    syslog(LOG_INFO, "sem_init(%d)\n", id);

    /* ... */

    return retval;
}

int sem_acquire(int id)
{
    int retval = 0;

    syslog(LOG_INFO, "sem_acquire(%d)\n", id);

    do {
        /* ... */
    } while (retval != 1);

    return retval;
}

int sem_release(int id)
{
    int retval = 0;

    syslog(LOG_INFO, "sem_release(%d)\n", id);

    /* ... */

    return retval;
}