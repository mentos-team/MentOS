/// @file abort.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "string.h"
#include "signal.h"
#include "stdio.h"

/// @brief Since there could be signal handlers listening for the abort, we need
/// to keep track at which stage of the abort we are.
static int stage = 0;

void abort(void)
{
    sigaction_t action;
    sigset_t sigset;

    /* Unblock SIGABRT.  */
    if (stage == 0) {
        ++stage;

        sigemptyset(&sigset);
        sigaddset(&sigset, SIGABRT);
        sigprocmask(SIG_UNBLOCK, &sigset, 0);
    }

    /* Send signal which possibly calls a user handler.  */
    if (stage == 1) {

        // We must allow recursive calls of abort
        int save_stage = stage;

        stage = 0;
        kill(getpid(), SIGABRT);
        stage = save_stage + 1;
    }

    /* There was a handler installed.  Now remove it.  */
    if (stage == 2) {
        ++stage;

        memset(&action, 0, sizeof(action));

        action.sa_handler = SIG_DFL;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;

        sigaction(SIGABRT, &action, NULL);
    }

    /* Try again.  */
    if (stage == 3) {
        ++stage;

        kill(getpid(), SIGABRT);
    }

    /* Now try to abort using the system specific command.  */
    if (stage == 4) {
        ++stage;

        __asm__ __volatile__("hlt");
    }

    /* If we can't signal ourselves and the abort instruction failed, exit.  */
    if (stage == 5) {
        ++stage;

        exit(127);
    }

    // If even this fails try to use the provided instruction to crash
    //  or otherwise make sure we never return.
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (1) __asm__ __volatile__("hlt");
#pragma clang diagnostic pop
}
