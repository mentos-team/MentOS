/// @file t_abort.c
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
    if (sig == SIGABRT) {

        static int counter = 0;
        counter += 1;

        printf("handler(%d) : Correct signal. ABRT (%d/3)\n", sig, counter);
        if (counter < 3)
            abort();

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

    if (sigaction(SIGABRT, &action, NULL) == -1) {
        printf("Failed to set signal handler (%s).\n", SIGABRT, strerror(errno));
        return 1;
    }

    abort();
}
