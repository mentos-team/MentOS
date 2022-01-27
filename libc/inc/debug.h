/// @file debug.h
/// @brief Debugging primitives.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#ifndef __DEBUG_HEADER__
/// Header for identifying outputs coming from a mechanism.
#define __DEBUG_HEADER__
#endif

/// @brief Extract the filename from the full path provided by __FILE__.
#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

/// @brief Prints the given character to debug output.
/// @param c The character to print.
void dbg_putchar(char c);

/// @brief Prints the given string to debug output.
/// @param s The string to print.
void dbg_puts(const char *s);

/// @brief Prints the given string to the debug output.
/// @param file   The name of the file.
/// @param fun    The name of the function.
/// @param line   The line inside the file.
/// @param format The format to used, see printf.
/// @param ...    The list of arguments.
void dbg_printf(const char *file, const char *fun, int line, const char *format, ...);

/// @brief Transforms the given amount of bytes to a readable string.
/// @param bytes The bytes to turn to string.
/// @return String representing the bytes in human readable form.
const char *to_human_size(unsigned long bytes);

/// @brief Transforms the given value to a binary string.
/// @param value to print.
/// @param length of the binary output.
/// @return String representing the binary value.
const char *dec_to_binary(unsigned long value, unsigned length);

/// Prints a debugging message.
#define pr_debug(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__ __VA_ARGS__)
