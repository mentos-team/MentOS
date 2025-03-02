/// @file waitpid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "sys/wait.h"
#include "system/syscall_types.h"
#include "unistd.h"

#include "errno.h"
#include "strerror.h"
#include "syslog.h"
#include "system/syscall_types.h"
#include "unistd.h"

pid_t waitpid(pid_t pid, int *status, int options)
{
#if 1
    pid_t __res;
    int __status = 0;
    do {
        __inline_syscall_3(__res, waitpid, pid, &__status, options);
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
#else

    pid_t ret;

    while (1) {
        __inline_syscall_3(ret, waitpid, pid, status, options);

        // Success: A child process state has changed.
        if (ret > 0) {
            break;
        }

        if (ret < 0) {
            __syscall_set_errno(ret);

            // Interrupted by a signal: Retry the syscall.
            if (errno == EINTR) {
                continue;
            }

            // No children to wait for, but WNOHANG allows non-blocking behavior.
            if (errno == ECHILD) {
                if (options & WNOHANG) {
                    // Return 0 to indicate no state change.
                    ret = 0;
                    break;
                }
                continue;
            }

            // Unrecoverable error: Return -1 and let the caller handle `errno`.
            break;
        }
    }

    // Return the PID of the child whose state has changed (or 0 for WNOHANG).
    return ret;
#endif
}

pid_t wait(int *status) { return waitpid(-1, status, 0); }
