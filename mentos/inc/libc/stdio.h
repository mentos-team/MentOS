///                MentOS, The Mentoring Operating system project
/// @file stdio.h
/// @brief Standard I/0 functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdarg.h"
#include "stddef.h"

/// @brief The maximum number of digits of an integer.
#define MAX_DIGITS_IN_INTEGER 11

#ifndef EOF
/// @brief Define the End-Of-File.
#define EOF (-1)
#endif

/// @brief Writes the given character to the standard output (stdout).
void putchar(int character);

/// @brief Writes the string pointed by str to the standard output (stdout)
///        and appends a newline character ('\n').
void puts(char *str);

/// @brief Returns the next character from the standard input (stdin).
int getchar(void);

/// @brief Reads characters from the standard input (stdin) and stores them
///        as a C string into str until a newline character or the end-of-file is
///        reached.
char *gets(char *str);

/// @brief Convert the given string to an integer.
int atoi(const char *str);

/// @brief Write formatted output to stdout.
int printf(const char *, ...);

/// @brief Read formatted input from stdin.
size_t scanf(const char *, ...);

/// @brief Write formatted output to the string str from argument list ARG.
int vsprintf(char *str, const char *fmt, va_list args);

/// @brief Write formatted output to the string str.
int sprintf(char *str, const char *fmt, ...);
