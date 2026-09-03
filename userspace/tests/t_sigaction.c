/// @file t_sigaction.c
/// @brief Demonstrates signal handling using `sigaction` to handle SIGUSR1.
/// @details The program sets up a handler for SIGUSR1 using `sigaction`, then
/// sends SIGUSR1 to itself using `kill`. After the handler is executed, it
/// prints the values allocated in the signal handler.
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
#include <unistd.h>

/// Pointer to store dynamically allocated values.
static int *values = NULL;

/// @brief Handler for SIGUSR1 signal.
/// @details Allocates memory for an array of integers and populates it with
/// values 0 to 3. Prints the values before and after memory allocation.
/// @param sig Signal number (in this case, SIGUSR1).
void sigusr1_handler(int sig)
{
    syslog(LOG_INFO, "[t_sigaction] handler(sig: %d) : Starting handler.\n", sig);
    syslog(LOG_INFO, "[t_sigaction] handler(sig: %d) : values pointer (before allocation): %p\n", sig, (void *)values);

    // Allocate memory for an array of 4 integers.
    values = malloc(sizeof(int) * 4);
    if (!values) {
        syslog(LOG_ERR, "[t_sigaction] Failed to allocate memory in signal handler: %s", strerror(errno));
        return;
    }

    // Populate the array with values and print them.
    for (int i = 0; i < 4; ++i) {
        values[i] = i;
        syslog(LOG_INFO, "[t_sigaction] values[%d] : `%d`\n", i, values[i]);
    }

    syslog(LOG_INFO, "[t_sigaction] handler(sig: %d) : values pointer (after allocation): %p\n", sig, (void *)values);
    syslog(LOG_INFO, "[t_sigaction] handler(sig: %d) : Ending handler.\n", sig);
}

int main(int argc, char *argv[])
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigusr1_handler; // Set handler for SIGUSR1.

    // Set the SIGUSR1 handler using sigaction.
    if (sigaction(SIGUSR1, &action, NULL) == -1) {
        syslog(LOG_ERR, "[t_sigaction] Failed to set signal handler for SIGUSR1: %s\n", strerror(errno));
        return 1;
    }

    // Display initial state before signal is sent.
    syslog(LOG_INFO, "[t_sigaction] main : Calling handler (signal %d).\n", SIGUSR1);
    syslog(LOG_INFO, "[t_sigaction] main : values pointer (before signal): %p\n", (void *)values);

    // Send SIGUSR1 to the current process.
    int ret = kill(getpid(), SIGUSR1);
    if (ret == -1) {
        syslog(LOG_ERR, "[t_sigaction] Failed to send SIGUSR1: %s", strerror(errno));
        return 1;
    }

    // Display state after signal handler execution.
    syslog(LOG_INFO, "[t_sigaction] main : Returning from handler (signal %d): %d.\n", SIGUSR1, ret);
    syslog(LOG_INFO, "[t_sigaction] main : values pointer (after signal): %p\n", (void *)values);

    // Print the array populated in the signal handler.
    if (values != NULL) {
        for (int i = 0; i < 4; ++i) {
            syslog(LOG_INFO, "[t_sigaction] values[%d] : `%d`\n", i, values[i]);
        }
    }

    // Free the allocated memory.
    free(values);

    return 0;
}
