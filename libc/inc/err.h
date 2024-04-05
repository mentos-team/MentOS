/// @file err.h
/// @brief Contains err functions
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include <stdarg.h>

/// @brief Print formatted error message on stderr and exit
/// @param eval The exit value.
/// @param fmt  The format string.
void err(int eval, const char *fmt, ...);
void verr(int eval, const char *fmt, va_list args);

/// @brief Print formatted message on stderr without appending an error message and exit
/// @param eval The exit value.
/// @param fmt  The format string.
void errx(int eval, const char *fmt, ...);
void verrx(int eval, const char *fmt, va_list args);
