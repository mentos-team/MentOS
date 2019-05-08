///                MentOS, The Mentoring Operating system project
/// @file nice.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "syscall.h"

int nice(int inc)
{
    ssize_t _res;

    DEFN_SYSCALL1(_res, __NR_nice, inc);

    return _res;
}
