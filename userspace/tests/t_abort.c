/// @file t_abort.c
/// @brief Demonstrates handling of the SIGABRT signal.
/// @details This program sets up a signal handler for the SIGABRT signal and
/// triggers the signal using the `abort` function.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

void sig_handler(int sig)
{
    syslog(LOG_INFO, "[t_abort] handler(%d) : Starting handler.\n", sig);
    if (sig == SIGABRT) {
        static int counter = 0;
        counter += 1;

        syslog(LOG_INFO, "[t_abort] handler(%d) : Correct signal. ABRT (%d/3)\n", sig, counter);
        if (counter < 3) {
            // Re-trigger the abort signal up to 3 times.
            abort();
        } else {
            // Exit the program after handling the signal 3 times.
            exit(EXIT_SUCCESS);
        }
    } else {
        syslog(LOG_INFO, "[t_abort] handler(%d) : Wrong signal.\n", sig);
    }
    syslog(LOG_INFO, "[t_abort] handler(%d) : Ending handler.\n", sig);
}

int main(int argc, char *argv[])
{
    sigaction_t action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sig_handler;

    // Set up the signal handler for SIGABRT.
    if (sigaction(SIGABRT, &action, NULL) == -1) {
        syslog(LOG_ERR, "[t_abort] signal setup failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Trigger the SIGABRT signal.
    abort();

    // This point should never be reached.
    syslog(LOG_ERR, "[t_abort] abort() failed to terminate the process: %s", strerror(errno));

    return EXIT_FAILURE;
}
