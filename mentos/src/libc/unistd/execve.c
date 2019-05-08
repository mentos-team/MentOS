///                MentOS, The Mentoring Operating system project
/// @file execve.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "syscall.h"

int execve(const char *path, char *const argv[], char *const envp[])
{
    ssize_t retval;

    DEFN_SYSCALL3(retval, __NR_execve, path, argv, envp);

    if (retval < 0)
    {
        errno = -retval;
        retval = -1;
    }

    return retval;
}
