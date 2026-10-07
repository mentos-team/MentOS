/// @file t_siginfo.c
/// @brief Demonstrates handling SIGFPE with siginfo structure to get detailed
/// signal information.
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

static volatile int sender_info_valid;

/// @brief Signal handler for SIGUSR1 that validates the sending task identity.
/// @param sig Signal number.
/// @param siginfo Information populated by kill().
static void sig_handler_sender(int sig, siginfo_t *siginfo)
{
    sender_info_valid = (sig == SIGUSR1 && siginfo != NULL && siginfo->si_code == SI_USER && siginfo->si_pid == getppid() && siginfo->si_uid == getuid());
}

/// @brief Verifies that kill() reports the calling task in siginfo_t.
/// @return 0 on success, 1 on failure.
static int test_signal_sender_info(void)
{
    int ready[2];
    if (pipe(ready) < 0) {
        return 1;
    }

    pid_t child = fork();
    if (child < 0) {
        close(ready[0]);
        close(ready[1]);
        return 1;
    }
    if (child == 0) {
        close(ready[0]);
        sigaction_t action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = (sighandler_t)sig_handler_sender;
        action.sa_flags   = SA_SIGINFO;
        if (sigaction(SIGUSR1, &action, NULL) < 0) {
            exit(EXIT_FAILURE);
        }
        char marker = 'R';
        if (write(ready[1], &marker, sizeof(marker)) != sizeof(marker)) {
            exit(EXIT_FAILURE);
        }
        close(ready[1]);

        timespec_t request = {2, 0};
        nanosleep(&request, NULL);
        exit(sender_info_valid ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    close(ready[1]);
    char marker = 0;
    int result  = 1;
    if (read(ready[0], &marker, sizeof(marker)) == sizeof(marker) && marker == 'R' && kill(child, SIGUSR1) == 0) {
        int status = 0;
        result     = (waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) ? 0 : 1;
    } else {
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
    }
    close(ready[0]);
    return result;
}

/// @brief Signal handler for SIGFPE that uses siginfo_t to get more information
/// about the signal.
/// @param sig Signal number.
/// @param siginfo Pointer to siginfo_t structure containing detailed
/// information about the signal.
void sig_handler_info(int sig, siginfo_t *siginfo)
{
    syslog(LOG_INFO, "[t_siginfo] handler(%d, %p) : Starting handler.\n", sig, (void *)siginfo);

    // Check if the received signal is SIGFPE.
    if (sig == SIGFPE) {
        syslog(LOG_INFO, "[t_siginfo] handler(%d, %p) : Correct signal.\n", sig, (void *)siginfo);

        // Print additional information from the siginfo structure.
        syslog(LOG_INFO, "[t_siginfo] handler(%d, %p) : Code : %d\n", sig, (void *)siginfo, siginfo->si_code);
        syslog(LOG_INFO, "[t_siginfo] handler(%d, %p) : Exiting\n", sig, (void *)siginfo);

        // Exit the process after handling the signal.
        exit(EXIT_SUCCESS);
    }

    // Handle unexpected signals.
    syslog(LOG_INFO, "[t_siginfo] handler(%d, %p) : Wrong signal.\n", sig, (void *)siginfo);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    if (test_signal_sender_info() != 0) {
        syslog(LOG_ERR, "[t_siginfo] kill() did not report the sending task\n");
        return EXIT_FAILURE;
    }

    sigaction_t action;

    // Initialize the sigaction structure with zeros.
    memset(&action, 0, sizeof(action));

    // Set the handler function and indicate that we want detailed information
    // (SA_SIGINFO).
    action.sa_handler = (sighandler_t)sig_handler_info;
    action.sa_flags   = SA_SIGINFO;

    // Attempt to set the signal handler for SIGFPE.
    if (sigaction(SIGFPE, &action, NULL) == -1) {
        // Print error message if sigaction fails.
        syslog(LOG_INFO, "[t_siginfo] Failed to set signal handler (%s).\n", strerror(errno));
        return 1;
    }

    syslog(LOG_INFO, "[t_siginfo] Diving by zero (unrecoverable)...\n");

    // Perform a division that will eventually cause a divide-by-zero error to
    // trigger SIGFPE.
    int d = 1;
    int e = 1;

    // Enter an infinite loop to progressively decrement e and eventually
    // trigger SIGFPE.
    while (1) {
        // Check to prevent division by zero for safety in other environments.
        if (e == 0) {
            syslog(LOG_ERR, "[t_siginfo] Attempt to divide by zero.\n");
            break;
        }
        d /= e;
        e -= 1;
    }

    // This line will not be reached if SIGFPE is triggered.
    syslog(LOG_INFO, "[t_siginfo] d: %d, e: %d\n", d, e);

    return EXIT_SUCCESS;
}
