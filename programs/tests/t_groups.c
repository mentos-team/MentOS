/// @file t_groups.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char **argv)
{
    pid_t gid = getgid();
    pid_t pid = getpid();
    pid_t sid = getsid(0);

    printf("pid: %d, gid: %d, sid: %d\n\n", pid, gid, sid);
    for (int i = 0; i < 5; ++i) {
        if (fork() == 0) {
            pid_t gid_child  = getgid();
            pid_t pid_child  = getpid();
            
            pid_t ppid_child = getppid();
            pid_t sid_child  = getsid(ppid_child);

            printf("%d) pid_child: %d, gid_child: %d, ppid_child: %d, sid_child: %d\n",
                   i, pid_child, gid_child, ppid_child, sid_child);
            sleep(5);
            exit(0);
        }
    }
    //int status;
    //while (wait(&status) > 0)
    //    ;

    return 0;
}
