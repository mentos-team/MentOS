/// @file waitpid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "sys/errno.h"
#include "sys/wait.h"
#include "system/syscall_types.h"

#include "sys/errno.h"
#include "sys/unistd.h"
#include "system/syscall_types.h"

pid_t waitpid(pid_t pid, int *status, int options)
{
    pid_t __res;
    int __status = 0;
    do {
        __inline_syscall3(__res, waitpid, pid, &__status, options);
        if (__res != 0) {
            break;
        }
        if (options && WNOHANG) {
            break;
        }
    } while (1);

    if (status) {
        *status = __status;
    }
    __syscall_return(pid_t, __res);
}

pid_t wait(int *status)
{
    return waitpid(-1, status, 0);
}
