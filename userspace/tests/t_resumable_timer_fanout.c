/// @file t_resumable_timer_fanout.c
/// @brief Test several timer-driven continuation wakeups in one parent.
/// @details Multiple children sleep for different intervals while the parent
///          waits for each one.  This exercises repeated timer IRQ dispatch,
///          wakeup ordering, and continuation resumption after the first
///          timer-driven context switch.
/// @copyright (c) 2024-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define CHILDREN 3

static int reap_child(pid_t child, int index)
{
    int status = 0;
    if (waitpid(child, &status, 0) != child) {
        syslog(LOG_ERR, "[t_resumable_timer_fanout] waitpid[%d] failed: %d", index, errno);
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != index) {
        syslog(LOG_ERR, "[t_resumable_timer_fanout] child[%d] status=0x%x", index, status);
        return 1;
    }
    return 0;
}

int main(void)
{
    pid_t children[CHILDREN];

    for (int index = 0; index < CHILDREN; ++index) {
        children[index] = fork();
        if (children[index] < 0) {
            syslog(LOG_ERR, "[t_resumable_timer_fanout] fork[%d] failed: %d", index, errno);
            return EXIT_FAILURE;
        }
        if (children[index] == 0) {
            timespec_t delay = {0, (long)(10000000 * (index + 1))};
            if (nanosleep(&delay, NULL) < 0) {
                exit(10 + index);
            }
            exit(index);
        }
    }

    for (int index = 0; index < CHILDREN; ++index) {
        syslog(LOG_INFO, "[t_resumable_timer_fanout] waiting for child %d", index);
        if (reap_child(children[index], index) != 0) {
            return EXIT_FAILURE;
        }
    }

    /* Confirm that timer IRQ delivery still works after all child wakeups. */
    timespec_t final_delay = {0, 20000000};
    if (nanosleep(&final_delay, NULL) < 0) {
        syslog(LOG_ERR, "[t_resumable_timer_fanout] final sleep failed: %d", errno);
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "[t_resumable_timer_fanout] all timer wakeups passed");
    return EXIT_SUCCESS;
}
