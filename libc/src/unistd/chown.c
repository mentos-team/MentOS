/// @file   chown.c
/// @brief
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall3(int, chown, const char *, pathname, uid_t, owner, gid_t, group)
int chown(const char *pathname, uid_t owner, gid_t group)
{
    long __res;
    __inline_syscall_3(__res, chown, pathname, owner, group);
    __syscall_return(int, __res);
}

// _syscall3(int, lchown, const char *, pathname, uid_t, owner, gid_t, group)
int lchown(const char *pathname, uid_t owner, gid_t group)
{
    long __res;
    __inline_syscall_3(__res, lchown, pathname, owner, group);
    __syscall_return(int, __res);
}

// _syscall3(int, fchown, int, fd, uid_t, owner, gid_t, group)
int fchown(int fd, uid_t owner, gid_t group)
{
    long __res;
    __inline_syscall_3(__res, fchown, fd, owner, group);
    __syscall_return(int, __res);
}
