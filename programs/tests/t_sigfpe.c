/// @file t_sigfpe.c
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
    if (sig == SIGFPE) {
        printf("handler(%d) : Correct signal. FPE\n", sig);
        printf("handler(%d) : Exiting\n", sig);
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

    if (sigaction(SIGFPE, &action, NULL) == -1) {
        printf("Failed to set signal handler (%s).\n", SIGFPE, strerror(errno));
        return 1;
    }

    printf("Diving by zero (unrecoverable)...\n");

    // Should trigger ALU error, fighting the compiler...
    int d = 1, e = 1;
    while (1) {
        d /= e;
        e -= 1;
    }
    printf("d: %d, e: %d\n", d, e);
}
