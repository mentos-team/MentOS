/// @file printk.c
/// @brief Functions for managing the kernel messages.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/printk.h"
#include "io/debug.h"

void sys_syslog(const char *file, const char *fun, int line, short log_level, const char *format)
{
    dbg_printf(file, fun, line, "[SYSLOG]", log_level, format);
}
