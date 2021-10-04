///                MentOS, The Mentoring Operating system project
/// @file printk.c
/// @brief Functions for managing the kernel messages.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "printk.h"
#include "stdarg.h"
#include "stdio.h"
#include "video.h"

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
