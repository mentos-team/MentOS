///                MentOS, The Mentoring Operating system project
/// @file   getcwd.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "syscall.h"
#include "errno.h"

void getcwd(char * path, size_t size)
{
    ssize_t retval;
    DEFN_SYSCALL2(retval, __NR_getcwd, path, size);
    if (retval < 0)
    {
        errno = -retval;
    }
}
