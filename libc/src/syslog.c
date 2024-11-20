/// @file   debug.c
/// @brief  Debugging primitives.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "syslog.h"

#include "system/syscall_types.h"
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
#include "stdio.h"

// #include "io/ansi_colors.h"
// #include "io/port_io.h"
// #include "math.h"
// #include "string.h"
// #include "sys/bitops.h"

// Global variables for syslog configuration
static int syslog_facility      = LOG_USER;
static int syslog_options       = 0;
static const char *syslog_ident = NULL;
static int log_mask             = 0xFF; // Mask allowing all levels by default

void openlog(const char *ident, int option, int facility)
{
    syslog_ident    = ident;
    syslog_options  = option;
    syslog_facility = facility;
}

int setlogmask(int mask)
{
    int old_mask = log_mask;
    log_mask     = mask;
    return old_mask;
}

void closelog(void)
{
    syslog_ident    = NULL;
    syslog_options  = 0;
    syslog_facility = LOG_USER;
    log_mask        = 0xFF; // Reset mask to allow all levels
}

int __syslog(const char *file, const char *fun, int line, short log_level, const char *format, ...)
{
    // Check if the message's priority is allowed by the log mask.
    if (!(log_mask & (1 << log_level))) {
        return 0;
    }

    // Buffer to hold the formatted message.
    char buf[BUFSIZ];
    int offset = 0;

    // Add identifier if specified.
    if (syslog_ident) {
        offset += snprintf(buf + offset, sizeof(buf) - offset, "%s: ", syslog_ident);
    }

    // Add PID if LOG_PID option is set.
    if (syslog_options & LOG_PID) {
        offset += snprintf(buf + offset, sizeof(buf) - offset, "[%d] ", getpid());
    }

    // Format the main log message with the variable arguments.
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buf + offset, sizeof(buf) - offset, format, args);
    va_end(args);

    // Adjust length if the message was truncated
    if (len >= (int)(sizeof(buf) - offset)) {
        len      = sizeof(buf) - 1;
        buf[len] = '\0'; // Null-terminate if truncated
    } else {
        len += offset; // Include the offset as part of the total length.
    }

    // Call the syslog system call to send the formatted message to the system log.
    // __inline_syscall_5(len, syslog, type, file, func, line, buf);
    __asm__ __volatile__("push %%ebx; movl %2,%%ebx; movl %1,%%eax; "
                         "int $0x80; pop %%ebx"
                         : "=a"(len)
                         : "i"(__NR_syslog), "ri"(file), "c"(fun), "d"(line), "S"(log_level), "D"(buf)
                         : "memory");

    // If the syslog system call fails and LOG_CONS is set, write to console as a fallback.
    if ((len == -1) && (syslog_options & LOG_CONS)) {
        fprintf(stderr, buf, len);
    }

    __syscall_return(int, len);
}
