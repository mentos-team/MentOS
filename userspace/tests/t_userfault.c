/// @file t_userfault.c
/// @brief Regression test for #237: user-mode page faults on kernel-mapped
/// addresses (and on present, non-CoW pages in general) must deliver
/// SIGSEGV to the faulting process instead of panicking the kernel.
/// @details Before the fix, a NULL dereference landed on the present,
/// supervisor-only PDE 0 (the first 1 MiB is identity-mapped), fell through
/// the copy-on-write branches of page_fault() and reached the unconditional
/// __page_fault_panic(): any program could take down the kernel with a wild
/// pointer. The test forks children that:
///   - dereference NULL (read and write);
///   - read and write the kernel image region (0xC0000000+), which also
///     covers the kernel mapping window branch of the handler;
/// and expects each child to die with SIGSEGV (WIFSIGNALED/WTERMSIG), or to
/// exit from an installed SIGSEGV handler. The suite continuing after the
/// children proves the kernel survived every fault.
/// A write to the child's own code page is not covered: PT_LOAD segments are
/// mapped writable regardless of their ELF permissions, so such a write
/// succeeds today (issue #210).
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

/// Exit code used by the installed handler to prove delivery.
#define HANDLER_EXIT 42

/// The kernel image region, supervisor-only for user mode.
#define KERNEL_REGION ((volatile int *)0xC0000000)

/// @brief SIGSEGV handler: exits with a marker code.
static void segv_handler(int sig)
{
    (void)sig;
    exit(HANDLER_EXIT);
}

/// @brief Runs the faulting operation in a child and checks the outcome.
/// @param label description of the case.
/// @param fault the faulting operation (executed by the child).
/// @param install_handler whether the child installs a SIGSEGV handler.
/// @return 0 on success (SIGSEGV death or handler exit), -1 on failure.
static int check_case(const char *label, void (*fault)(void), int install_handler)
{
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "[t_userfault] fork: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        if (install_handler && (signal(SIGSEGV, segv_handler) == SIG_ERR)) {
            syslog(LOG_ERR, "[t_userfault] signal: %s", strerror(errno));
            exit(90);
        }
        fault();
        // Reaching this point means the fault did not happen or was silently
        // serviced: both are wrong.
        exit(91);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        syslog(LOG_ERR, "[t_userfault] waitpid: %s", strerror(errno));
        return -1;
    }
    if (install_handler) {
        if (WIFEXITED(status) && (WEXITSTATUS(status) == HANDLER_EXIT)) {
            return 0;
        }
        syslog(
            LOG_ERR, "[t_userfault] %s: expected handler exit %d, got raw status 0x%x", label, HANDLER_EXIT, status);
        return -1;
    }
    if (WIFSIGNALED(status) && (WTERMSIG(status) == SIGSEGV)) {
        return 0;
    }
    syslog(LOG_ERR, "[t_userfault] %s: expected SIGSEGV, got raw status 0x%x", label, status);
    return -1;
}

/// @brief Reads through a NULL pointer.
static void fault_null_read(void)
{
    volatile int *p = (volatile int *)0;
    int v           = *p;
    (void)v;
}

/// @brief Writes through a NULL pointer.
static void fault_null_write(void)
{
    volatile int *p = (volatile int *)0;
    *p              = 1;
}

/// @brief Reads from the kernel image region.
static void fault_kernel_read(void)
{
    int v = *KERNEL_REGION;
    (void)v;
}

/// @brief Writes to the kernel image region.
static void fault_kernel_write(void)
{
    *KERNEL_REGION = 1;
}

int main(void)
{
    int failures = 0;

    // Without a handler, each fault must kill the child with SIGSEGV.
    if (check_case("null read", fault_null_read, 0) < 0) {
        ++failures;
    }
    if (check_case("null write", fault_null_write, 0) < 0) {
        ++failures;
    }
    if (check_case("kernel read", fault_kernel_read, 0) < 0) {
        ++failures;
    }
    if (check_case("kernel write", fault_kernel_write, 0) < 0) {
        ++failures;
    }
    // With a handler installed, the signal must be delivered through the
    // faulting frame and the handler must run.
    if (check_case("null read, handled", fault_null_read, 1) < 0) {
        ++failures;
    }
    if (check_case("kernel read, handled", fault_kernel_read, 1) < 0) {
        ++failures;
    }

    if (failures == 0) {
        syslog(LOG_INFO, "[t_userfault] all user-mode fault checks passed");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_userfault] %d FAILURES", failures);
    return EXIT_FAILURE;
}
