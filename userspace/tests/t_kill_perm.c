/// @file t_kill_perm.c
/// @brief Test that kill() enforces the POSIX permission rule.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#define UNPRIVILEGED_UID 1000

int main(int argc, char *argv[])
{
    pid_t parent = getpid();
    pid_t child  = fork();
    if (child < 0) {
        syslog(LOG_ERR, "[t_kill_perm] fork failed.\n");
        return EXIT_FAILURE;
    }
    if (child == 0) {
        // Drop privileges: from now on the child shares no uid with the parent.
        if (setuid(UNPRIVILEGED_UID) != 0) {
            syslog(LOG_ERR, "[t_kill_perm] setuid failed.\n");
            exit(EXIT_FAILURE);
        }
        // Signal 0 performs only the permission check.
        if ((kill(getpid(), 0) != 0)) {
            syslog(LOG_ERR, "[t_kill_perm] kill(self, 0) must succeed.\n");
            exit(EXIT_FAILURE);
        }
        if ((kill(parent, 0) != -1) || (errno != EPERM)) {
            syslog(LOG_ERR, "[t_kill_perm] unprivileged kill(parent, 0) must fail with EPERM.\n");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    // The privileged parent may signal the unprivileged child.
    if (kill(child, 0) != 0) {
        syslog(LOG_ERR, "[t_kill_perm] privileged kill(child, 0) must succeed.\n");
        return EXIT_FAILURE;
    }
    int status = 0;
    if ((waitpid(child, &status, 0) != child) || !WIFEXITED(status) || (WEXITSTATUS(status) != EXIT_SUCCESS)) {
        syslog(LOG_ERR, "[t_kill_perm] child reported a failure.\n");
        return EXIT_FAILURE;
    }
    syslog(LOG_INFO, "[t_kill_perm] kill() permission rule enforced.\n");
    return EXIT_SUCCESS;
}
