/// @file debug.h
/// @brief Debugging primitives.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/kernel_levels.h"

#ifndef __DEBUG_LEVEL__
/// Defines the debug level, by default we set it to notice.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE
#endif

#ifndef __DEBUG_HEADER__
/// Header for identifying outputs coming from a mechanism.
#define __DEBUG_HEADER__ 0
#endif

/// @brief Extract the filename from the full path provided by __FILE__.
#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

/// @brief Sets the loglevel.
/// @param level The new loglevel.
void set_log_level(int level);

/// @brief Returns the current loglevel.
/// @return The current loglevel
int get_log_level(void);

/// @brief Prints the given character to debug output.
/// @param c The character to print.
void dbg_putchar(char c);

/// @brief Prints the given string to debug output.
/// @param s The string to print.
void dbg_puts(const char *s);

/// @brief Prints the given string to the debug output.
/// @param file the name of the file.
/// @param fun the name of the function.
/// @param line the line inside the file.
/// @param header the header to print.
/// @param log_level the log level.
/// @param format the format to used, see printf.
/// @param ... the list of arguments.
void dbg_printf(const char *file, const char *fun, int line, char *header, short log_level, const char *format, ...);

/// @brief Transforms the given amount of bytes to a readable string.
/// @param bytes The bytes to turn to string.
/// @return String representing the bytes in human readable form.
const char *to_human_size(unsigned long bytes);

/// @brief Transforms the given value to a binary string.
/// @param value to print.
/// @param length of the binary output.
/// @return String representing the binary value.
const char *dec_to_binary(unsigned long value, unsigned length);

/// Prints a default message, which is always shown.
#define pr_default(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_DEFAULT, __VA_ARGS__)

/// Prints an emergency message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_EMERG
#define pr_emerg(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_EMERG, __VA_ARGS__)
#else
#define pr_emerg(...)
#endif

/// Prints an alert message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_ALERT
#define pr_alert(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_ALERT, __VA_ARGS__)
#else
#define pr_alert(...)
#endif

/// Prints a critical message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_CRIT
#define pr_crit(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_CRIT, __VA_ARGS__)
#else
#define pr_crit(...)
#endif

/// Prints an error message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_ERR
#define pr_err(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_ERR, __VA_ARGS__)
#else
#define pr_err(...)
#endif

/// Prints a warning message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_WARNING
#define pr_warning(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_WARNING, __VA_ARGS__)
#else
#define pr_warning(...)
#endif

/// Prints a notice message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_NOTICE
#define pr_notice(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_NOTICE, __VA_ARGS__)
#else
#define pr_notice(...)
#endif

/// Prints a info message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_INFO
#define pr_info(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_INFO, __VA_ARGS__)
#else
#define pr_info(...)
#endif

/// Prints a debug message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_DEBUG
#define pr_debug(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_DEBUG, __VA_ARGS__)
#else
#define pr_debug(...)
#endif

#ifdef __KERNEL__

struct pt_regs;

/// @brief Prints the registers on debug output.
/// @param frame Pointer to the register.
void dbg_print_regs(struct pt_regs *frame);

#endif
