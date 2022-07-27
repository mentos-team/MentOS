/// @file printk.c
/// @brief Functions for managing the kernel messages.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/printk.h"
#include "stdarg.h"
#include "stdio.h"
#include "io/video.h"

int sys_syslog(const char *format, ...)
{
    char buffer[4096];
    va_list ap;
    // Start variabile argument's list.
    va_start(ap, format);
    int len = vsprintf(buffer, format, ap);
    va_end(ap);
    video_puts(buffer);
    return len;
}
