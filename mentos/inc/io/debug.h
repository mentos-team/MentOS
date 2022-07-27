/// @file debug.h
/// @brief Debugging primitives.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/kernel_levels.h"

struct pt_regs;

#ifndef __DEBUG_LEVEL__
/// Defines the debug level.
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
int get_log_level();

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
/// @param header The header to print.
/// @param format The format to used, see printf.
/// @param ...    The list of arguments.
void dbg_printf(const char *file, const char *fun, int line, char *header, const char *format, ...);

/// @brief Prints the registers on debug output.
/// @param frame Pointer to the register.
void dbg_print_regs(struct pt_regs *frame);

/// @brief Transforms the given amount of bytes to a readable string.
/// @param bytes The bytes to turn to string.
/// @return String representing the bytes in human readable form.
const char *to_human_size(unsigned long bytes);

/// @brief Transforms the given value to a binary string.
/// @param value to print.
/// @param length of the binary output.
/// @return String representing the binary value.
const char *dec_to_binary(unsigned long value, unsigned length);

/// Prints a KERN_DEFAULT.
#define pr_default(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, KERN_DEFAULT __VA_ARGS__)
/// Prints a KERN_EMERG.
#if __DEBUG_LEVEL__ >= LOGLEVEL_EMERG
#define pr_emerg(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, KERN_EMERG __VA_ARGS__)
#else
#define pr_emerg(...)
#endif
/// Prints a KERN_ALERT.
#if __DEBUG_LEVEL__ >= LOGLEVEL_ALERT
#define pr_alert(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, KERN_ALERT __VA_ARGS__)
#else
#define pr_alert(...)
#endif
/// Prints a KERN_CRIT.
#if __DEBUG_LEVEL__ >= LOGLEVEL_CRIT
#define pr_crit(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, KERN_CRIT __VA_ARGS__)
#else
#define pr_crit(...)
#endif
/// Prints a KERN_ERR.
#if __DEBUG_LEVEL__ >= LOGLEVEL_ERR
#define pr_err(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, KERN_ERR __VA_ARGS__)
#else
#define pr_err(...)
#endif
/// Prints a KERN_WARNING.
#if __DEBUG_LEVEL__ >= LOGLEVEL_WARNING
#define pr_warning(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, KERN_WARNING __VA_ARGS__)
#else
#define pr_warning(...)
#endif
/// Prints a KERN_NOTICE.
#if __DEBUG_LEVEL__ >= LOGLEVEL_NOTICE
#define pr_notice(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, KERN_NOTICE __VA_ARGS__)
#else
#define pr_notice(...)
#endif
/// Prints a KERN_INFO.
#if __DEBUG_LEVEL__ >= LOGLEVEL_INFO
#define pr_info(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, KERN_INFO __VA_ARGS__)
#else
#define pr_info(...)
#endif
/// Prints a KERN_DEBUG.
#if __DEBUG_LEVEL__ >= LOGLEVEL_DEBUG
#define pr_debug(...) dbg_printf(__FILENAME__, __func__, __LINE__, __DEBUG_HEADER__, KERN_DEBUG __VA_ARGS__)
#else
#define pr_debug(...)
#endif
