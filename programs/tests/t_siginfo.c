/// @file t_siginfo.c
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

void sig_handler_info(int sig, siginfo_t *siginfo)
{
    printf("handler(%d, %p) : Starting handler.\n", sig, siginfo);
    if (sig == SIGFPE) {
        printf("handler(%d, %p) : Correct signal.\n", sig, siginfo);
        printf("handler(%d, %p) : Code : %d\n", sig, siginfo, siginfo->si_code);
        printf("handler(%d, %p) : Exiting\n", sig, siginfo);
        exit(1);
    } else {
        printf("handler(%d, %p) : Wrong signal.\n", sig, siginfo);
    }
    printf("handler(%d, %p) : Ending handler.\n", sig, siginfo);
}

int main(int argc, char *argv[])
{
    sigaction_t action;
    memset(&action, 0, sizeof(action));

    action.sa_handler = (sighandler_t)sig_handler_info;
    action.sa_flags   = SA_SIGINFO;

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
