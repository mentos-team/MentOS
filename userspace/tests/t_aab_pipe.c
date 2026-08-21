/// @file t_aab_pipe.c
/// @brief Regression test for issue #195: sys_pipe() under fd exhaustion
///        must fail cleanly without freeing pipe_inode_info multiple times.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define MAX_OPENS 64

int main(void)
{
    openlog("t_aab_pipe", LOG_CONS | LOG_PID, LOG_USER);

    // Exhaust the fd table so that get_unused_fd fails inside sys_pipe.
    int opened[MAX_OPENS];
    int count = 0;
    int fd;
    while ((count < MAX_OPENS) && ((fd = open("/", O_RDONLY, 0)) >= 0)) {
        opened[count++] = fd;
    }
    syslog(LOG_INFO, "exhausted fds after %d opens (errno=%d)\n", count, errno);
    if (count == 0) {
        syslog(LOG_ERR, "[t_aab_pipe] failed to exhaust fds\n");
        return 1;
    }

    // pipe() must fail while no fd can be allocated.
    int fds[2] = { -1, -1 };
    int ret     = pipe(fds);
    syslog(LOG_INFO, "pipe() returned %d (fds: %d, %d)\n", ret, fds[0], fds[1]);
    if (ret != -1) {
        syslog(LOG_ERR, "[t_aab_pipe] pipe() unexpectedly succeeded\n");
        if (fds[0] >= 0) {
            close(fds[0]);
        }
        if (fds[1] >= 0) {
            close(fds[1]);
        }
        for (int i = 0; i < count; ++i) {
            close(opened[i]);
        }
        return 1;
    }

    // Release the exhausted fds.
    for (int i = 0; i < count; ++i) {
        close(opened[i]);
    }

    // The pipe subsystem must still be fully functional after the failed
    // creation: a fresh pipe must carry data end to end.
    if (pipe(fds) == -1) {
        syslog(LOG_ERR, "[t_aab_pipe] pipe() failed after fd exhaustion\n");
        return 1;
    }
    const char msg[] = "survived";
    char buf[sizeof(msg)] = { 0 };
    if ((write(fds[1], msg, sizeof(msg)) != (ssize_t)sizeof(msg)) ||
        (read(fds[0], buf, sizeof(buf)) != (ssize_t)sizeof(msg)) ||
        (strcmp(buf, msg) != 0)) {
        syslog(LOG_ERR, "[t_aab_pipe] post-failure pipe is not functional\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }
    close(fds[0]);
    close(fds[1]);

    syslog(LOG_INFO, "[t_aab_pipe] all checks passed\n");
    return 0;
}
