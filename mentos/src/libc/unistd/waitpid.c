///                MentOS, The Mentoring Operating system project
/// @file waitpid.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "wait.h"
#include "errno.h"
#include "syscall.h"
#include "scheduler.h"

pid_t wait(int *status)
{
    return waitpid(-1, status, 0);
}

pid_t waitpid(pid_t pid, int *status, int options)
{
    pid_t retval;

    int _status = 0, *_status_ptr = &_status;

    do
    {
        DEFN_SYSCALL3(retval, __NR_waitpid, pid, _status_ptr, options);
    } while (((*_status_ptr) != EXIT_ZOMBIE) && !(options && WNOHANG));

    if (status != NULL)
    {
        (*status) = (*_status_ptr);
    }

    if (retval < 0)
    {
        errno = -retval;
        retval = -1;
    }

    return retval;
}
