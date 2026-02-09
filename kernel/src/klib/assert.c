/// @file assert.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "assert.h"
#include "stdio.h"
#include "system/panic.h"

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[ASSERT]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

void __assert_fail(const char *assertion, const char *file, const char *function, unsigned int line)
{
    pr_emerg(
        "\n=== ASSERTION FAILED ===\n"
        "Assertion: %s\n"
        "Location : %s:%d\n"
        "Function : %s\n\n",
        assertion, file, line, (function ? function : "Unknown function"));
    kernel_panic("Assertion failed.");
}
