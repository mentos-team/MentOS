/// @file t_syscall_ni.c
/// @brief Test program for unimplemented system-call numbers.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <syslog.h>
#include <system/syscall_types.h>

/// @brief Issues a system-call with no arguments, bypassing the libc wrappers.
/// @param nr The system-call number to invoke.
/// @return The raw value returned by the kernel in eax.
static long raw_syscall0(uint32_t nr)
{
    long res;
    __asm__ __volatile__("int $0x80" : "=a"(res) : "0"(nr));
    return res;
}

/// @brief Checks that a system-call number returns -ENOSYS.
/// @param nr The system-call number to invoke.
/// @param description A human-readable description of the number.
/// @return 1 if the call returned -ENOSYS, 0 otherwise.
static int expect_enosys(uint32_t nr, const char *description)
{
    long res = raw_syscall0(nr);
    if (res != -ENOSYS) {
        syslog(LOG_ERR, "syscall %u (%s) returned %ld, expected -ENOSYS (%d)\n", nr, description, res, -ENOSYS);
        return 0;
    }
    syslog(LOG_INFO, "syscall %u (%s) returned -ENOSYS as expected\n", nr, description);
    return 1;
}

int main(void)
{
    openlog("t_syscall_ni", LOG_CONS | LOG_PID, LOG_USER);

    int success = 1;

    // A number which is in range, unregistered, and follows the standard
    // argument convention must fail cleanly instead of dispatching to NULL.
    success &= expect_enosys(__NR_readv, "in range, standard args");

    // `clone` is in range and takes the frame-pointer special path in the
    // dispatcher, but has no registered handler.
    success &= expect_enosys(__NR_clone, "in range, frame-pointer special case");

    // An out-of-range number must keep returning -ENOSYS.
    success &= expect_enosys(SYSCALL_NUMBER, "out of range");

    // An out-of-range number expressed as a "negative" value must also be
    // rejected by the unsigned range check.
    success &= expect_enosys(UINT32_MAX, "negative/out of range");

    if (!success) {
        syslog(LOG_ERR, "t_syscall_ni failed\n");
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "t_syscall_ni passed\n");
    return EXIT_SUCCESS;
}
