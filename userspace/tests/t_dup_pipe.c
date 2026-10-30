/// @file t_dup_pipe.c
/// @brief Tests that a duplicated pipe descriptor keeps the pipe writable.
/// @details A child moves the write end onto stdout with `close` + `dup` and
/// closes the original. The reader must see the child's data, not an
/// immediate end of file.
/// @copyright (c) 2024-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

int main(void)
{
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        syslog(LOG_ERR, "[t_dup_pipe] pipe: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "[t_dup_pipe] fork: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        close(pipefd[0]);
        close(STDOUT_FILENO);
        if (dup(pipefd[1]) != STDOUT_FILENO) {
            exit(EXIT_FAILURE);
        }
        close(pipefd[1]);
        // Give the parent time to reach read() and to close its write end.
        for (volatile int i = 0; i < 2000000; ++i) {
        }
        if (write(STDOUT_FILENO, "hi", 2) != 2) {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    close(pipefd[1]);
    // Ask for exactly what was written: pipe_read mishandles short reads.
    char buffer[4] = {0};
    ssize_t count  = read(pipefd[0], buffer, 2);
    close(pipefd[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) != pid) {
        syslog(LOG_ERR, "[t_dup_pipe] waitpid: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0)) {
        syslog(LOG_ERR, "[t_dup_pipe] child failed: status=%d", status);
        return EXIT_FAILURE;
    }
    if ((count != 2) || strncmp(buffer, "hi", 2)) {
        syslog(LOG_ERR, "[t_dup_pipe] read returned %d, expected 2 bytes 'hi'", (int)count);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
