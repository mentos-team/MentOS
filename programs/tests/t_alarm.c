/// @file t_alarm.c
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

void alarm_handler(int sig)
{
    printf("handler(%d) : Starting handler.\n", sig);
    if (sig == SIGALRM) {
        printf("handler(%d) : Correct signal.\n", sig);

        alarm(5);
        int rest = alarm(5);
        printf("handler(%d) : alarm(5) result: %d.\n", sig, rest);

        rest = alarm(0);
        printf("handler(%d) : alarm(0) result: %d.\n", sig, rest);

        exit(0);

    } else {
        printf("handler(%d) : Wrong signal.\n", sig);
    }
    printf("handler(%d) : Ending handler.\n", sig);
}

int main(int argc, char *argv[])
{
    sigaction_t action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = alarm_handler;
    if (sigaction(SIGALRM, &action, NULL) == -1) {
        printf("Failed to set signal handler (%s).\n", SIGALRM, strerror(errno));
        return 1;
    }


    alarm(5);
    while(1) { }

    return 0;
}
