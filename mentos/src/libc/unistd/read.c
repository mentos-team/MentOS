///                MentOS, The Mentoring Operating system project
/// @file read.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "syscall.h"

ssize_t read(int fd, void *buf, size_t nbytes)
{
    ssize_t retval;

    DEFN_SYSCALL3(retval, __NR_read, fd, buf, nbytes);

    if (retval < 0)
    {
        errno = -retval;
        retval = -1;
    }

    return retval;
}
