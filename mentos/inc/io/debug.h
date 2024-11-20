/// @file debug.h
/// @brief Debugging primitives.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/kernel_levels.h"
#include "string.h"

#ifndef __DEBUG_LEVEL__
/// Defines the debug level, by default we set it to notice.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE
#endif

#ifndef __DEBUG_HEADER__
/// Header for identifying outputs coming from a mechanism.
#define __DEBUG_HEADER__ 0
#endif

/// @brief Sets the loglevel.
/// @param level The new loglevel.
void set_log_level(int level);

/// @brief Transforms the given amount of bytes to a readable string.
/// @param bytes The bytes to turn to string.
/// @return String representing the bytes in human readable form.
const char *to_human_size(unsigned long bytes);

/// @brief Transforms the given value to a binary string.
/// @param value to print.
/// @param length of the binary output.
/// @return String representing the binary value.
const char *dec_to_binary(unsigned long value, unsigned length);

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

/// @brief Extracts the relative path of the current file from the project root.
///
/// This macro calculates the relative path of the file (`__FILE__`) by skipping
/// the prefix defined by `MENTOS_ROOT`. It is used to simplify file path
/// logging by removing the absolute path up to the project root.
///
/// @note Ensure that `MENTOS_ROOT` is correctly defined as the root path of the
/// project. If `__FILE__` does not start with `MENTOS_ROOT`, the behavior is
/// undefined.
///
/// @example
/// If
///     MENTOS_ROOT = "/path/to/mentos" and
///     __FILE__    = "/path/to/mentos/src/kernel/main.c", the result will be
///                                   "src/kernel/main.c".
#define __RELATIVE_PATH__ \
    (strncmp(__FILE__, MENTOS_ROOT, sizeof(MENTOS_ROOT) - 1) == 0 ? (&__FILE__[sizeof(MENTOS_ROOT)]) : __FILE__)

/// General logging macro that logs a message at the specified log level.
/// Only logs messages if the specified log level is less than or equal to __DEBUG_LEVEL__.
#define pr_log(level, ...)                                                                           \
    do {                                                                                             \
        if (level <= __DEBUG_LEVEL__) {                                                              \
            dbg_printf(__RELATIVE_PATH__, __func__, __LINE__, __DEBUG_HEADER__, level, __VA_ARGS__); \
        }                                                                                            \
    } while (0)

/// Prints a default message, which is always shown.
#define pr_default(...) dbg_printf(__RELATIVE_PATH__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_DEFAULT, __VA_ARGS__)

/// Prints an emergency message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_EMERG
#define pr_emerg(...) dbg_printf(__RELATIVE_PATH__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_EMERG, __VA_ARGS__)
#else
#define pr_emerg(...)
#endif

/// Prints an alert message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_ALERT
#define pr_alert(...) dbg_printf(__RELATIVE_PATH__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_ALERT, __VA_ARGS__)
#else
#define pr_alert(...)
#endif

/// Prints a critical message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_CRIT
#define pr_crit(...) dbg_printf(__RELATIVE_PATH__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_CRIT, __VA_ARGS__)
#else
#define pr_crit(...)
#endif

/// Prints an error message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_ERR
#define pr_err(...) dbg_printf(__RELATIVE_PATH__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_ERR, __VA_ARGS__)
#else
#define pr_err(...)
#endif

/// Prints a warning message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_WARNING
#define pr_warning(...) dbg_printf(__RELATIVE_PATH__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_WARNING, __VA_ARGS__)
#else
#define pr_warning(...)
#endif

/// Prints a notice message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_NOTICE
#define pr_notice(...) dbg_printf(__RELATIVE_PATH__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_NOTICE, __VA_ARGS__)
#else
#define pr_notice(...)
#endif

/// Prints a info message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_INFO
#define pr_info(...) dbg_printf(__RELATIVE_PATH__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_INFO, __VA_ARGS__)
#else
#define pr_info(...)
#endif

/// Prints a debug message.
#if __DEBUG_LEVEL__ >= LOGLEVEL_DEBUG
#define pr_debug(...) dbg_printf(__RELATIVE_PATH__, __func__, __LINE__, __DEBUG_HEADER__, LOGLEVEL_DEBUG, __VA_ARGS__)
#else
#define pr_debug(...)
#endif

struct pt_regs;
/// @brief Prints the registers using the specified debug function.
/// @param dbg_fn The debug function to use for printing.
/// @param frame The registers to print.
#define PRINT_REGS(dbg_fn, frame)                     \
    do {                                              \
        dbg_fn("Interrupt stack frame:\n");           \
        dbg_fn("GS     = 0x%-04x\n", frame->gs);      \
        dbg_fn("FS     = 0x%-04x\n", frame->fs);      \
        dbg_fn("ES     = 0x%-04x\n", frame->es);      \
        dbg_fn("DS     = 0x%-04x\n", frame->ds);      \
        dbg_fn("EDI    = 0x%-09x\n", frame->edi);     \
        dbg_fn("ESI    = 0x%-09x\n", frame->esi);     \
        dbg_fn("EBP    = 0x%-09x\n", frame->ebp);     \
        dbg_fn("ESP    = 0x%-09x\n", frame->esp);     \
        dbg_fn("EBX    = 0x%-09x\n", frame->ebx);     \
        dbg_fn("EDX    = 0x%-09x\n", frame->edx);     \
        dbg_fn("ECX    = 0x%-09x\n", frame->ecx);     \
        dbg_fn("EAX    = 0x%-09x\n", frame->eax);     \
        dbg_fn("INT_NO = %-9d\n", frame->int_no);     \
        dbg_fn("ERR_CD = %-9d\n", frame->err_code);   \
        dbg_fn("EIP    = 0x%-09x\n", frame->eip);     \
        dbg_fn("CS     = 0x%-04x\n", frame->cs);      \
        dbg_fn("EFLAGS = 0x%-09x\n", frame->eflags);  \
        dbg_fn("UESP   = 0x%-09x\n", frame->useresp); \
        dbg_fn("SS     = 0x%-04x\n", frame->ss);      \
    } while (0)
