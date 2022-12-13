/// @file t_kill.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strerror.h>
#include <sys/wait.h>

#define cpu_relax() __asm__ __volatile__("pause\n" \
                                 :         \
                                 :         \
                                 : "memory")

static inline void fake_sleep(int times)
{
    for (int j = 0; j < times; ++j)
        for (long i = 1; i < 1024000; ++i)
            cpu_relax();
}

void child_sigusr1_handler(int sig)
{
    printf("handler(sig: %d) : Starting handler (pid: %d).\n", sig, getpid());
    printf("handler(sig: %d) : Ending handler (pid: %d).\n", sig, getpid());
}

void child_process()
{
    while (1) {
        printf("I'm the child (%d): I'm playing around!\n", getpid());
        fake_sleep(1);
    }
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
        child_process();
    } else {
        printf("I'm the parent (%d)!\n", ppid);
    }
    fake_sleep(9);
    kill(ppid, SIGUSR1);
    fake_sleep(9);
    kill(ppid, SIGTERM);
    int status;
    wait(&status);
    printf("main : end (%d)\n", status);
    return 0;
}
