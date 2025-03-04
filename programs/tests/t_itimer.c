/// @file t_itimer.c
/// @brief Test the interval timer (itimerval) functionality.
/// @details This program sets an interval timer that triggers a signal handler
/// every second. The signal handler counts the number of times it is called and
/// stops the timer after 4 calls. It demonstrates the use of `setitimer` and
/// `getitimer` functions to manage interval timers.
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

// Global variables to track timer events
volatile int timer_fired = 0;
int timer_count          = 0;

// Signal handler for the timer
void timer_handler(int signum)
{
    timer_fired = 1;
    timer_count++;
}

int main(void)
{
    struct itimerval timer;
    time_t start_time;
    time_t current_time;

    // Set up the signal handler for SIGALRM
    signal(SIGALRM, timer_handler);

    // Configure the timer to expire after 250 ms
    timer.it_value.tv_sec  = 0;      // Initial expiration (seconds)
    timer.it_value.tv_usec = 250000; // Initial expiration (microseconds)

    // Configure periodicity: 250 ms interval
    timer.it_interval.tv_sec  = 0;      // Periodic interval (seconds)
    timer.it_interval.tv_usec = 250000; // Periodic interval (microseconds)

    // Start the timer
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("setitimer");
        return EXIT_FAILURE;
    }

    // Record the start time
    start_time = time(NULL);

    // Test periodicity: wait for 5 timer events
    while (timer_count < 5) {
        if (timer_fired) {
            timer_fired = 0; // Reset the flag

            // Record the current time
            current_time = time(NULL);

            // Print the elapsed time since the start
            printf("Timer event %d fired at %u seconds since start\n", timer_count, current_time - start_time);
        }
    }

    // Stop the timer
    timer.it_value.tv_sec     = 0;
    timer.it_value.tv_usec    = 0;
    timer.it_interval.tv_sec  = 0;
    timer.it_interval.tv_usec = 0;
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("setitimer (stop)");
        return EXIT_FAILURE;
    }

    printf("Test completed: Timer fired %d times\n", timer_count);

    return EXIT_SUCCESS;
}
