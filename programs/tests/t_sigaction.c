/// @file t_sigaction.c
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

static int *values = NULL;

void sigusr1_handler(int sig)
{
    printf("handler(sig: %d) : Starting handler.\n", sig);
    printf("handler(sig: %d) : value : %d\n", sig, values);
    values = malloc(sizeof(int) * 4);
    for (int i = 0; i < 4; ++i) {
        values[i] = i;
        printf("values[%d] : `%d`\n", i, values[i]);
    }
    printf("handler(sig: %d) : value : %d\n", sig, values);
    printf("handler(sig: %d) : Ending handler.\n", sig);
}

int main(int argc, char *argv[])
{
    sigaction_t action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigusr1_handler;
    if (sigaction(SIGUSR1, &action, NULL) == -1) {
        printf("Failed to set signal handler (%s).\n", SIGUSR1, strerror(errno));
        return 1;
    }

    printf("main : Calling handler (%d).\n", SIGUSR1);
    printf("main : value : %d\n", values);
    int ret = kill(getpid(), SIGUSR1);
    printf("main : Returning from handler (%d): %d.\n", SIGUSR1, ret);
    printf("main : value : %d\n", values);
    for (int i = 0; i < 4; ++i)
        printf("values[%d] : `%d`\n", i, values[i]);
    free(values);
    return 0;
}
