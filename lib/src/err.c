/// @file err.h
/// @brief Contains err functions
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void verr(int status, const char *format, va_list args)
{
    if (format) {
        vfprintf(STDERR_FILENO, format, args);
        fprintf(STDERR_FILENO, ": ");
    }
    perror(0);
    exit(status);
}

void verrx(int status, const char *format, va_list args)
{
    if (format) {
        vfprintf(STDERR_FILENO, format, args);
    }
    fprintf(STDERR_FILENO, "\n");
    exit(status);
}

void err(int status, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    verr(status, format, ap);
    va_end(ap);
}

void errx(int status, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    verrx(status, format, ap);
    va_end(ap);
}
