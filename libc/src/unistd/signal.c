/// @file signal.c
/// @brief
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "system/syscall_types.h"
#include "sys/errno.h"

#include "signal.h"
#include "sys/bitops.h"

static const char *sys_siglist[] = {
    "HUP",
    "INT",
    "QUIT",
    "ILL",
    "TRAP",
    "ABRT",
    "EMT",
    "FPE",
    "KILL",
    "BUS",
    "SEGV",
    "SYS",
    "PIPE",
    "ALRM",
    "TERM",
    "USR1",
    "USR2",
    "CHLD",
    "PWR",
    "WINCH",
    "URG",
    "POLL",
    "STOP",
    "TSTP",
    "CONT",
    "TTIN",
    "TTOU",
    "VTALRM",
    "PROF",
    "XCPU",
    "XFSZ",
    NULL,
};

_syscall2(sighandler_t, signal, int, signum, sighandler_t, handler)

_syscall3(int, sigaction, int, signum, const sigaction_t *, act, sigaction_t *, oldact)

_syscall3(int, sigprocmask, int, how, const sigset_t *, set, sigset_t *, oldset)

const char *strsignal(int sig)
{
    if ((sig >= SIGHUP) && (sig < NSIG))
        return sys_siglist[sig - 1];
    return NULL;
}

int sigemptyset(sigset_t *set)
{
    if (set) {
        set->sig[0] = 0;
        return 0;
    }
    return -1;
}

int sigfillset(sigset_t *set)
{
    if (set) {
        set->sig[0] = ~0UL;
        return 0;
    }
    return -1;
}

int sigaddset(sigset_t *set, int signum)
{
    if (set && ((signum))) {
        bit_set_assign(set->sig[(signum - 1) / 32], ((signum - 1) % 32));
        return 0;
    }
    return -1;
}

int sigdelset(sigset_t *set, int signum)
{
    if (set) {
        bit_clear_assign(set->sig[(signum - 1) / 32], ((signum - 1) % 32));
        return 0;
    }
    return -1;
}

int sigismember(sigset_t *set, int signum)
{
    if (set)
        return bit_check(set->sig[(signum - 1) / 32], (signum - 1) % 32);
    return -1;
}