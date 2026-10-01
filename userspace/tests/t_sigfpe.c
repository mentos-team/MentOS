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

/// Signal handler function that catches and handles SIGFPE.
void sig_handler(int sig)
{
    syslog(LOG_INFO, "[t_sigfpe] handler(%d) : Starting handler.\n", sig);
    if (sig == SIGFPE) {
        syslog(LOG_INFO, "[t_sigfpe] handler(%d) : Correct signal. FPE\n", sig);
        syslog(LOG_INFO, "[t_sigfpe] handler(%d) : Exiting\n", sig);
        exit(0);
    }
    // Any other signal means the kernel is mapping the division-by-zero
    // exception to the wrong signal again (#415). Fail loudly instead of
    // exiting 0, so a regression shows up as a test failure rather than
    // being silently tolerated.
    syslog(LOG_INFO, "[t_sigfpe] handler(%d) : Wrong signal, expected SIGFPE (%d).\n", sig, SIGFPE);
    exit(1);
}

int main(int argc, char *argv[])
{
    sigaction_t action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sig_handler;

    // Set the SIGFPE handler using sigaction.
    if (sigaction(SIGFPE, &action, NULL) == -1) {
        syslog(LOG_INFO, "[t_sigfpe] Failed to set signal handler (%s).\n", strerror(errno));
        return 1;
    }

    syslog(LOG_INFO, "[t_sigfpe] Diving by zero (unrecoverable)...\n");

    // Should trigger ALU error, fighting the compiler...
    int d = 1;
    int e = 1;
    d /= e;
    e -= 1;
    d /= e;
    e -= 1;
    syslog(LOG_INFO, "[t_sigfpe] d: %d, e: %d\n", d, e);

    return EXIT_SUCCESS;
}
