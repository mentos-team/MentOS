/// @file waitpid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "sys/wait.h"
#include "system/syscall_types.h"
#include "unistd.h"

#include "strerror.h"
#include "syslog.h"

pid_t waitpid(pid_t pid, int *status, int options)
{
    pid_t __res;
    int __status = 0;
    do {
        __inline_syscall_3(__res, waitpid, pid, &__status, options);
        
        // Success: A child process was reaped.
        if (__res > 0) {
            break;
        }
        
        // WNOHANG was set and no child changed state - return immediately.
        if ((options & WNOHANG) && __res == 0) {
            break;
        }
        
        // If the syscall was interrupted by a signal (EINTR), retry it.
        // This happens when we were sleeping in waitpid and SIGCHLD woke us up.
        if (__res == -EINTR) {
            continue;
        }
        
        // Any other error (ECHILD, ESRCH, etc.) should be returned.
        if (__res < 0) {
            break;
        }
    } while (1);

    if (status) {
        *status = __status;
    }
    __syscall_return(pid_t, __res);
}

pid_t wait(int *status) { return waitpid(-1, status, 0); }
