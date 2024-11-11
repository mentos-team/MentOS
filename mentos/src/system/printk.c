/// @file printk.c
/// @brief Functions for managing the kernel messages.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/printk.h"

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[SYSLOG]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

int sys_syslog(int type, char *buf, int len)
{
    if (type == LOGLEVEL_EMERG) {
        pr_emerg("%s\n", buf);
    } else if (type == LOGLEVEL_ALERT) {
        pr_alert("%s\n", buf);
    } else if (type == LOGLEVEL_CRIT) {
        pr_crit("%s\n", buf);
    } else if (type == LOGLEVEL_ERR) {
        pr_err("%s\n", buf);
    } else if (type == LOGLEVEL_WARNING) {
        pr_warning("%s\n", buf);
    } else if (type == LOGLEVEL_NOTICE) {
        pr_notice("%s\n", buf);
    } else if (type == LOGLEVEL_INFO) {
        pr_info("%s\n", buf);
    } else if (type == LOGLEVEL_DEBUG) {
        pr_debug("%s\n", buf);
    } else {
        pr_default("%s\n", buf);
    }
    return len;
}
