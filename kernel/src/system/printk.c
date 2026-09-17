/// @file printk.c
/// @brief Functions for managing the kernel messages.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/printk.h"
#include "io/debug.h"
#include "mem/paging.h"

void sys_syslog(const char *file, const char *fun, int line, short log_level, const char *format)
{
    // All three strings must live in the caller's memory before the kernel
    // reads one byte of them (#191); the bounds are what the logging entry
    // points actually print of each.
    if ((strnlen_user(file, 256) < 0) || (strnlen_user(fun, 256) < 0) || (strnlen_user(format, 1024) < 0)) {
        return;
    }
    // The message comes from user space, so it is an argument and never the
    // format. Passing it as the format let a caller supply conversion
    // specifiers to the kernel's own printf, against a variadic list that
    // does not exist — `%n` writes through a pointer read from the kernel
    // stack past the real parameters, and `%s` dereferences one (#351).
    dbg_printf(file, fun, line, "[SYSLOG]", log_level, "%s", format);
}
