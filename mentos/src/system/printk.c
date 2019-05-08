///                MentOS, The Mentoring Operating system project
/// @file printk.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "printk.h"
#include "stdarg.h"
#include "stdio.h"
#include "video.h"

void printk(const char * format, ...)
{
    char buffer[4096];
    va_list ap;
    // Start variabile argument's list.
    va_start (ap, format);
    int len = vsprintf(buffer, format, ap);
    va_end (ap);

    for (size_t i = 0; (i < len); ++i)
        video_putc(buffer[i]);
}
