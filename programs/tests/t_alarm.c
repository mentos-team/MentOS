/// @file t_alarm.c
/// @brief Demonstrates handling of the SIGALRM signal.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/// @brief Signal handler for SIGALRM.
/// @param sig The signal number.
void alarm_handler(int sig)
{
    printf("handler(%d) : Starting handler.\n", sig);
    if (sig == SIGALRM) {
        // Set an alarm to go off after 1 seconds.
        alarm(1);

        // Set another alarm to go off after 1 seconds and get the remaining time of the previous alarm.
        int rest = alarm(1);

        // Expected value:  1 (since the previous alarm was just set to 1 seconds).
        printf("handler(%d) : alarm(1) result: %d.\n", sig, rest);

        // Cancel the alarm and get the remaining time of the previous alarm.
        rest = alarm(0);

        // Expected value: ~4 (since the previous alarm was just set to 1
        // seconds again). This small delay between the two alarm calls is why
        // you see the value 4 instead of 1. The exact value can vary slightly
        // depending on the system’s execution speed and the time taken to
        // execute the intermediate code.
        printf("handler(%d) : alarm(0) result: %d.\n", sig, rest);

        exit(EXIT_SUCCESS);
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

    // Set up the signal handler for SIGALRM.
    if (sigaction(SIGALRM, &action, NULL) < 0) {
        perror("signal setup failed");
        exit(EXIT_FAILURE);
    }

    // Set an alarm to go off after 1 seconds.
    alarm(1);

    // Infinite loop to keep the program running until the alarm signal is received.
    while (1) {
    }

    return EXIT_SUCCESS;
}
