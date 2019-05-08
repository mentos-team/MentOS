///                MentOS, The Mentoring Operating system project
/// @file exit.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "types.h"
#include "syscall.h"

void exit(int status) {
    int _res;

    DEFN_SYSCALL1(_res, __NR_exit, status);

    // The process never returns from this system call!
}
