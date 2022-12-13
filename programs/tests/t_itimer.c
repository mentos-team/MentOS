/// @file t_itimer.c
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

void alarm_handler(int sig)
{
    printf("handler(%d) : Starting handler.\n", sig);
    if (sig == SIGALRM) {

        itimerval val = { 0 };
        getitimer(ITIMER_REAL, &val);
        printf("(sec: %d, usec: %d)\n", val.it_interval.tv_sec, val.it_interval.tv_usec);

        static int counter = 0;
        counter += 1;

        printf("handler(%d) : Correct signal x%d\n", sig, counter);
        if (counter == 4)
        {
            itimerval interval = { 0 }, prev = { 0 };
            interval.it_interval.tv_sec = 0;

            setitimer(ITIMER_REAL, &interval, &prev);
            printf("prev: (sec: %d, usec: %d)", prev.it_interval.tv_sec, prev.it_interval.tv_usec);

            exit(0);
        }

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

    itimerval interval = { 0 };
    interval.it_interval.tv_sec = 4;
    setitimer(ITIMER_REAL, &interval, NULL);

    while(1) { }
    return 0;
}
