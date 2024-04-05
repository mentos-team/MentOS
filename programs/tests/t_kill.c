/// @file t_kill.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strerror.h>
#include <sys/wait.h>
#include <time.h>

void child_sigusr1_handler(int sig)
{
    printf("handler(sig: %d) : Starting handler (pid: %d).\n", sig, getpid());
    printf("handler(sig: %d) : Ending handler (pid: %d).\n", sig, getpid());
}

int main(int argc, char *argv[])
{
    printf("main : Creating child!\n");
    pid_t ppid;
    if ((ppid = fork()) == 0) {
        printf("I'm the child (%d)!\n", ppid);
        sigaction_t action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = child_sigusr1_handler;
        if (sigaction(SIGUSR1, &action, NULL) == -1) {
            printf("Failed to set signal handler (%s).\n", SIGUSR1, strerror(errno));
            return 1;
        }
        while (1) {
            printf("I'm the child (%d): I'm playing around!\n", getpid());
            sleep(1);
        }
    } else {
        printf("I'm the parent (%d)!\n", ppid);
    }
    sleep(2);
    kill(ppid, SIGUSR1);
    sleep(2);
    kill(ppid, SIGTERM);
    wait(NULL);
    printf("main : end\n");
    return 0;
}
