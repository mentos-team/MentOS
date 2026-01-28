/// @file t_groups.c
/// @brief Test process group and session IDs.
/// @details This program tests the retrieval of process IDs, group IDs, and
/// session IDs. It forks multiple child processes, each of which prints its own
/// IDs and those of its parent. The parent process waits for all child
/// processes to finish before exiting. This demonstrates the relationship
/// between parent and child processes in terms of IDs.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    // Get the group ID, process ID, and session ID of the current process.
    pid_t gid = getgid();
    pid_t pid = getpid();
    pid_t sid = getsid(0);

    // Print the IDs of the current process.
    printf("pid: %d, gid: %d, sid: %d\n\n", pid, gid, sid);

    // Fork 5 child processes.
    for (int i = 0; i < 5; ++i) {
        if (fork() == 0) {
            // In the child process, get the group ID, process ID, parent process ID, and session ID.
            pid_t gid_child  = getgid();
            pid_t pid_child  = getpid();
            pid_t ppid_child = getppid();
            pid_t sid_child  = getsid(ppid_child);

            // Request to sleep for (i * 100 ms).
            struct timespec req = {0, i * 100000000};
            nanosleep(&req, NULL);

            // Print the IDs of the child process and its parent.
            printf(
                "%d) pid_child: %d, gid_child: %d, ppid_child: %d, sid_child: "
                "%d\n",
                i, pid_child, gid_child, ppid_child, sid_child);
            exit(EXIT_SUCCESS);
        }
    }

    // Wait for all child processes to finish.
    while (wait(NULL) != -1) {
    }

    return EXIT_SUCCESS;
}
