/// @file err.h
/// @brief Contains err functions
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
#include <err.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

void verr(int status, const char *fmt, va_list ap)
{
	if (fmt) {
		vfprintf(STDERR_FILENO, fmt, ap);
		fprintf(STDERR_FILENO, ": ");
	}
	perror(0);
	exit(status);
}

void verrx(int status, const char *fmt, va_list ap)
{
	if (fmt) vfprintf(STDERR_FILENO, fmt, ap);
	fprintf(STDERR_FILENO, "\n");
	exit(status);
}

void err(int status, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verr(status, fmt, ap);
	va_end(ap);
}

void errx(int status, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verrx(status, fmt, ap);
	va_end(ap);
}
