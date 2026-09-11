/// @file printk.c
/// @brief Functions for managing the kernel messages.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/printk.h"
#include "io/debug.h"

void sys_syslog(const char *file, const char *fun, int line, short log_level, const char *format)
{
    // The message comes from user space, so it is an argument and never the
    // format. Passing it as the format let a caller supply conversion
    // specifiers to the kernel's own printf, against a variadic list that
    // does not exist — `%n` writes through a pointer read from the kernel
    // stack past the real parameters, and `%s` dereferences one (#351).
    dbg_printf(file, fun, line, "[SYSLOG]", log_level, "%s", format);
}
