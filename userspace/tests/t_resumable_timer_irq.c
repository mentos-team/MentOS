/// @file t_resumable_timer_irq.c
/// @brief Regression test for timer IRQs that resume blocked continuations.
/// @details Each cycle makes a child sleep while its parent blocks in waitpid,
///          then performs another sleep in the parent.  The second sleep is
///          important: losing the PIC EOI during the first timer-driven
///          continuation switch would leave all later timer IRQs masked.
/// @copyright (c) 2024-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define CYCLES 3
#define SLEEP_NS 20000000

static int sleep_once(void)
{
    timespec_t delay = {0, SLEEP_NS};
    return nanosleep(&delay, NULL) < 0;
}

static int wait_successfully(pid_t child)
{
    int status = 0;
    if (waitpid(child, &status, 0) != child) {
        syslog(LOG_ERR, "[t_resumable_timer_irq] waitpid failed: %d", errno);
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        syslog(LOG_ERR, "[t_resumable_timer_irq] child status=0x%x", status);
        return 1;
    }
    return 0;
}

static int timer_wait_cycle(int cycle)
{
    pid_t child = fork();
    if (child < 0) {
        syslog(LOG_ERR, "[t_resumable_timer_irq] fork failed: %d", errno);
        return 1;
    }
    if (child == 0) {
        if (sleep_once() != 0) {
            exit(2);
        }
        exit(0);
    }

    syslog(LOG_INFO, "[t_resumable_timer_irq] cycle %d: waitpid blocks", cycle);
    if (wait_successfully(child) != 0) {
        return 1;
    }
    syslog(LOG_INFO, "[t_resumable_timer_irq] cycle %d: timer wakeup resumed waitpid", cycle);

    /* A second timer interrupt proves that the first IRQ completed cleanly. */
    if (sleep_once() != 0) {
        syslog(LOG_ERR, "[t_resumable_timer_irq] cycle %d: second sleep failed: %d", cycle, errno);
        return 1;
    }
    return 0;
}

static int pipe_wait_cycle(int cycle)
{
    int fds[2];
    if (pipe(fds) < 0) {
        syslog(LOG_ERR, "[t_resumable_timer_irq] cycle %d: pipe failed: %d", cycle, errno);
        return 1;
    }

    pid_t child = fork();
    if (child < 0) {
        close(fds[0]);
        close(fds[1]);
        return 1;
    }
    if (child == 0) {
        char value = 0;
        close(fds[1]);
        ssize_t count = read(fds[0], &value, sizeof(value));
        close(fds[0]);
        exit(count == 1 && value == (char)cycle ? 0 : 2);
    }

    close(fds[0]);
    if (sleep_once() != 0) {
        close(fds[1]);
        return 1;
    }
    char value = (char)cycle;
    if (write(fds[1], &value, sizeof(value)) != 1) {
        close(fds[1]);
        return 1;
    }
    close(fds[1]);
    return wait_successfully(child);
}

int main(void)
{
    for (int cycle = 0; cycle < CYCLES; ++cycle) {
        if (timer_wait_cycle(cycle) != 0 || pipe_wait_cycle(cycle) != 0) {
            syslog(LOG_ERR, "[t_resumable_timer_irq] failed at cycle %d", cycle);
            return EXIT_FAILURE;
        }
    }
    syslog(LOG_INFO, "[t_resumable_timer_irq] timer IRQ and pipe wakeup checks passed");
    return EXIT_SUCCESS;
}
