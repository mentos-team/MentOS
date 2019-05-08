///                MentOS, The Mentoring Operating system project
/// @file getpid.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"

#include "syscall.h"
#include "types.h"

pid_t getpid()
{
    pid_t ret;

    DEFN_SYSCALL0(ret, __NR_getpid);

    return ret;
}
