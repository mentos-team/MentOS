/// @file t_sigmask.c
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

void sigusr1_handler(int sig)
{
    printf("handler(sig: %d) : Starting handler.\n", sig);
    printf("handler(sig: %d) : Ending handler.\n", sig);
}

int main(int argc, char *argv[])
{
    int ret;
    sigaction_t action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigusr1_handler;
    if (sigaction(SIGUSR1, &action, NULL) == -1) {
        printf("Failed to set signal handler (%d, %s).\n", SIGUSR1, strerror(errno));
        return 1;
    }

    printf("main : Blocking signal (%d).\n", SIGUSR1);
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    printf("main : Calling handler (%d).\n", SIGUSR1);
    ret = kill(getpid(), SIGUSR1);
    printf("main : Returning from handler (%d): %d.\n", SIGUSR1, ret);

    printf("main : Unblocking signal (%d).\n", SIGUSR1);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    printf("main : Calling handler (%d).\n", SIGUSR1);
    ret = kill(getpid(), SIGUSR1);
    printf("main : Returning from handler (%d): %d.\n", SIGUSR1, ret);

    return 0;
}
