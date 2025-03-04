/// @file err.h
/// @brief Contains err functions
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include <stdarg.h>

/// @brief Print a formatted error message on stderr and exit the program.
///
/// @details This function prints an error message to stderr, formatted
/// according to the given format string, followed by a system error message if
/// applicable. It then terminates the program with the specified exit value.
/// This is typically used when a system call fails.
///
/// @param status The exit value to use when terminating the program.
/// @param format The format string for the error message.
void err(int status, const char *format, ...);

/// @brief Print a formatted error message on stderr using a va_list and exit
/// the program.
///
/// @details This function is similar to `err()`, but accepts a `va_list` to
/// support variable arguments. This allows you to pass a list of arguments that
/// can be formatted into the error message. The program exits with the
/// specified exit value.
///
/// @param status The exit value to use when terminating the program.
/// @param format The format string for the error message.
/// @param args The variable argument list.
void verr(int status, const char *format, va_list args);

/// @brief Print a formatted message on stderr without appending an error
/// message and exit.
///
/// @details This function prints a formatted message to stderr without
/// appending a system error message (such as those related to errno). It then
/// terminates the program with the specified exit value. This is useful when
/// the error isn't related to a system call failure but requires exiting the
/// program.
///
/// @param status The exit value to use when terminating the program.
/// @param format The format string for the message.
void errx(int status, const char *format, ...);

/// @brief Print a formatted message on stderr using a va_list and exit without
/// appending an error message.
///
/// @details This function is similar to `errx()`, but accepts a `va_list` to
/// handle variable arguments. It prints the formatted message and exits the
/// program without appending a system error message.
///
/// @param status The exit value to use when terminating the program.
/// @param format The format string for the message.
/// @param args The variable argument list.
void verrx(int status, const char *format, va_list args);
