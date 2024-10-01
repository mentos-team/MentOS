/// @file t_alarm.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <sys/wait.h>
#include <strerror.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

/// @brief Signal handler for SIGALRM.
/// @param sig The signal number.
void alarm_handler(int sig)
{
    if (sig == SIGALRM) {
        // Set an alarm to go off after 5 seconds.
        alarm(5);

        // Set another alarm to go off after 5 seconds and get the remaining time of the previous alarm.
        int rest = alarm(5);

        // Expected value:  5 (since the previous alarm was just set to 5 seconds).
        printf("handler(%d) : alarm(5) result: %d.\n", sig, rest);

        // Cancel the alarm and get the remaining time of the previous alarm.
        rest = alarm(0);

        // Expected value: ~4 (since the previous alarm was just set to 5
        // seconds again). This small delay between the two alarm calls is why
        // you see the value 4 instead of 5. The exact value can vary slightly
        // depending on the system’s execution speed and the time taken to
        // execute the intermediate code.
        printf("handler(%d) : alarm(0) result: %d.\n", sig, rest);

        exit(EXIT_SUCCESS);
    }
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

    // Set an alarm to go off after 5 seconds.
    alarm(5);

    // Infinite loop to keep the program running until the alarm signal is received.
    while (1) {}

    return EXIT_SUCCESS;
}
