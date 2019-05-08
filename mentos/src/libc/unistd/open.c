///                MentOS, The Mentoring Operating system project
/// @file open.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "syscall.h"

int open(const char *pathname, int flags, mode_t mode)
{
    ssize_t retval;

    DEFN_SYSCALL3(retval, __NR_open, pathname, flags, mode);

    if (retval < 0)
    {
        errno = -retval;
        retval = -1;

    }
    return retval;
}
