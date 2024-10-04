/// @file t_itimer.c
/// @brief Test the interval timer (itimerval) functionality.
/// @details This program sets an interval timer that triggers a signal handler
/// every second. The signal handler counts the number of times it is called and
/// stops the timer after 4 calls. It demonstrates the use of `setitimer` and
/// `getitimer` functions to manage interval timers.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <strerror.h>
#include <sys/wait.h>
#include <sys/unistd.h>

/// @brief Signal handler for SIGALRM.
/// @param sig The signal number.
void alarm_handler(int sig)
{
    printf("handler(%d) : Starting handler.\n", sig);
    if (sig == SIGALRM) {
        struct itimerval val = { 0 };
        // Get the current value of the interval timer.
        if (getitimer(ITIMER_REAL, &val) == -1) {
            perror("getitimer failed");
            exit(EXIT_FAILURE);
        }
        printf("(sec: %ld, usec: %ld)\n", val.it_interval.tv_sec, val.it_interval.tv_usec);

        static int counter = 0;
        counter += 1;

        printf("handler(%d) : Correct signal x%d\n", sig, counter);
        if (counter == 4) {
            struct itimerval interval = { 0 }, prev = { 0 };
            // Stop the interval timer.
            if (setitimer(ITIMER_REAL, &interval, &prev) == -1) {
                perror("setitimer failed");
                exit(EXIT_FAILURE);
            }
            printf("prev: (sec: %ld, usec: %ld)\n", prev.it_interval.tv_sec, prev.it_interval.tv_usec);
            exit(EXIT_SUCCESS);
        }
    } else {
        printf("handler(%d) : Wrong signal.\n", sig);
    }
    printf("handler(%d) : Ending handler.\n", sig);
}

int main(int argc, char *argv[])
{
    struct sigaction action;
    struct itimerval timer;

    memset(&action, 0, sizeof(action));
    memset(&timer, 0, sizeof(timer));

    action.sa_handler = alarm_handler;

    // Set up the signal handler for SIGALRM.
    if (sigaction(SIGALRM, &action, NULL) == -1) {
        fprintf(STDERR_FILENO, "Failed to set signal handler: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Configure the timer to expire after 1 second and then every 1 second.
    timer.it_value.tv_sec     = 1; // Initial delay.
    timer.it_value.tv_usec    = 0;
    timer.it_interval.tv_sec  = 1; // Interval for periodic timer.
    timer.it_interval.tv_usec = 0;

    // Start the interval timer.
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("setitimer failed");
        return EXIT_FAILURE;
    }

    // Infinite loop to keep the program running until the timer stops.
    while (1) {}

    return EXIT_SUCCESS;
}
