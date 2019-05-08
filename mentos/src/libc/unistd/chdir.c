///                MentOS, The Mentoring Operating system project
/// @file   chdir.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "syscall.h"
#include "errno.h"

void chdir(char const * path)
{
    ssize_t retval;
    DEFN_SYSCALL1(retval, __NR_chdir, path);
    if (retval < 0)
    {
        errno = -retval;
    }
}
