/// @file t_sigfpe.c
/// @brief Demonstrates handling of a SIGFPE (floating-point exception) signal
/// using sigaction. The program intentionally triggers a division by zero to
/// cause the SIGFPE signal.
/// @details
/// This program sets a signal handler for the SIGFPE signal, which is raised
/// when a floating-point exception occurs (in this case, division by zero).
/// When the signal is received, the handler function is invoked, which catches
/// the exception, displays relevant messages, and then exits. The program
/// contains a section that deliberately divides by zero to trigger this signal.
///
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strerror.h>
#include <sys/wait.h>
#include <time.h>

/// Signal handler function that catches and handles SIGFPE.
void sig_handler(int sig)
{
    printf("handler(%d) : Starting handler.\n", sig);
    if (sig == SIGFPE) {
        printf("handler(%d) : Correct signal. FPE\n", sig);
        printf("handler(%d) : Exiting\n", sig);
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
    action.sa_handler = sig_handler;

    // Set the SIGUSR1 handler using sigaction.
    if (sigaction(SIGFPE, &action, NULL) == -1) {
        printf("Failed to set signal handler (%s).\n", SIGFPE, strerror(errno));
        return 1;
    }

    printf("Diving by zero (unrecoverable)...\n");

    // Should trigger ALU error, fighting the compiler...
    int d = 1, e = 1;
    d /= e;
    e -= 1;
    d /= e;
    e -= 1;
    printf("d: %d, e: %d\n", d, e);

    return EXIT_SUCCESS;
}
