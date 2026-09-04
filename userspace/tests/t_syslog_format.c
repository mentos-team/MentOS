/// @file t_syslog_format.c
/// @brief Regression test for #351: a message passed to syslog is data, never
/// a format string.
/// @details `sys_syslog` handed the user-supplied message straight to
/// `dbg_printf` as its format argument, with no variadic arguments after it:
///
///     dbg_printf(file, fun, line, "[SYSLOG]", log_level, format);
///
/// So any conversion specifier in the message was executed by the kernel's own
/// printf against an argument list that did not exist. `%s` dereferenced a
/// pointer read from the kernel stack past the real parameters, and `%n` —
/// which that printf implemented — *wrote* through one.
///
/// The test sends the specifiers that mattered and requires the process to
/// come back and the system to keep running. It cannot inspect the kernel log
/// from user space, so what it proves is bounded: that the specifiers reach
/// the kernel and are survived. The write primitive itself is closed by
/// removing `%n` from the kernel's printf, which is a compile-time fact rather
/// than something a test can observe.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

int main(void)
{
    openlog("t_syslog_format", LOG_PID | LOG_CONS, LOG_USER);

    // Each of these arrives at the kernel as message *text*. Before the fix
    // the kernel read them as conversions: %n took a pointer off its own
    // stack and wrote to it, %s dereferenced one, and %99999999d asked it to
    // pad to a length no buffer has.
    syslog(LOG_INFO, "[t_syslog_format] literal percent-n: %%n");
    syslog(LOG_INFO, "[t_syslog_format] literal percent-s: %%s");
    syslog(LOG_INFO, "[t_syslog_format] several: %%n %%s %%p %%x");
    syslog(LOG_INFO, "[t_syslog_format] wide: %%99999999d");
    syslog(LOG_INFO, "[t_syslog_format] trailing percent: %%");

    // And the same by the route a program is most likely to hit it: the
    // specifier arriving inside an argument rather than written in the format.
    const char *from_argument = "%n%n%n%s%s%s";
    syslog(LOG_INFO, "[t_syslog_format] from an argument: %s", from_argument);

    // Reaching here means every one of those returned. The suite continuing
    // past this test is what shows the kernel survived them.
    syslog(LOG_INFO, "[t_syslog_format] the kernel treated every message as text");
    closelog();
    return EXIT_SUCCESS;
}
