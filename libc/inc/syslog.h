/// @file printk.c
/// @brief Functions for kernel log messages.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "string.h"

// Log levels for setting the severity of log messages.
#define LOG_EMERG   0 ///< Emergency: system unusable.
#define LOG_ALERT   1 ///< Alert: immediate action required.
#define LOG_CRIT    2 ///< Critical: critical conditions.
#define LOG_ERR     3 ///< Error: error conditions.
#define LOG_WARNING 4 ///< Warning: warning conditions.
#define LOG_NOTICE  5 ///< Notice: significant condition.
#define LOG_INFO    6 ///< Info: informational messages.
#define LOG_DEBUG   7 ///< Debug: debugging messages.

// Option flags for syslog behavior.
#define LOG_CONS 0x01 ///< Log to console if there are issues with logging
#define LOG_PID  0x02 ///< Include the process ID with each log message

// Log facilities.
#define LOG_KERN     (0 << 3)  ///< Kernel messages
#define LOG_USER     (1 << 3)  ///< User-level messages
#define LOG_MAIL     (2 << 3)  ///< Mail system
#define LOG_DAEMON   (3 << 3)  ///< System daemons
#define LOG_AUTH     (4 << 3)  ///< Security/authorization messages
#define LOG_SYSLOG   (5 << 3)  ///< Messages generated internally by syslogd
#define LOG_LPR      (6 << 3)  ///< Printer subsystem
#define LOG_NEWS     (7 << 3)  ///< Network news subsystem
#define LOG_UUCP     (8 << 3)  ///< UUCP subsystem
#define LOG_CRON     (9 << 3)  ///< Clock daemon (cron and at)
#define LOG_AUTHPRIV (10 << 3) ///< Security/authorization (private)
#define LOG_FTP      (11 << 3) ///< FTP daemon

/// @brief Creates a log mask that includes all priorities up to the specified level.
#define LOG_UPTO(pri) ((1 << ((pri) + 1)) - 1)

/// @brief Opens a connection to the system log
/// @param ident Identifier string for log messages
/// @param option Flags for logging options, e.g., LOG_PID to include the process ID
/// @param facility The log facility, e.g., LOG_USER for user-level messages
void openlog(const char *ident, int option, int facility);

/// @brief Sets the log level mask to control which messages are logged
/// @param mask Bitmask for allowed log levels; use LOG_UPTO to set a maximum level
/// @return The previous log mask
int setlogmask(int mask);

/// @brief Closes the syslog connection and resets log settings
void closelog(void);

/// @brief Sends a formatted message to the system log
/// @param file the name of the file.
/// @param fun the name of the function.
/// @param line the line inside the file.
/// @param log_level the log level.
/// @param format the format to used, see printf.
/// @param ... Arguments for the format string
/// @return The number of bytes written or -1 on failure
int __syslog(const char *file, const char *fun, int line, short log_level, const char *format, ...);

/// @brief Extracts the relative path of the current file from the project root.
/// @details This macro calculates the relative path of the file (`__FILE__`) by
/// skipping the prefix defined by `MENTOS_ROOT`. It is used to simplify file
/// path logging by removing the absolute path up to the project root.
/// If
///     MENTOS_ROOT = "/path/to/mentos" and
///     __FILE__    = "/path/to/mentos/src/kernel/main.c", the result will be
///                                   "src/kernel/main.c".
#define __RELATIVE_PATH__ \
    (strncmp(__FILE__, MENTOS_ROOT, sizeof(MENTOS_ROOT) - 1) == 0 ? (&__FILE__[sizeof(MENTOS_ROOT)]) : __FILE__)

/// @brief Wrapper macro to simplify usage.
#define syslog(...) __syslog(__RELATIVE_PATH__, __func__, __LINE__, __VA_ARGS__)
