/// @file t_sigusr.c
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
#include <time.h>

void sig_handler(int sig)
{
    printf("handler(%d) : Starting handler.\n", sig);
    
    static int counter = 0;
    if (sig == SIGUSR1 || sig == SIGUSR2) {
        printf("handler(%d) : Correct signal. SIGUSER\n", sig);
        counter += 1;

        if (counter == 2)
            exit(1);

    } else {
        printf("handler(%d) : Wrong signal.\n", sig);
    }
    printf("handler(%d) : Ending handler.\n", sig);
}

int main(int argc, char *argv[])
{
    sigaction_t action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sig_handler;

    if (sigaction(SIGUSR1, &action, NULL) == -1) {
        printf("Failed to set signal handler (%s).\n", SIGUSR1, strerror(errno));
        return 1;
    }

    if (sigaction(SIGUSR2, &action, NULL) == -1) {
        printf("Failed to set signal handler (%s).\n", SIGUSR2, strerror(errno));
        return 1;
    }

    kill(getpid(), SIGUSR1);
    sleep(2);
    kill(getpid(), SIGUSR2);

    while(1) { };
    return 0;
}
