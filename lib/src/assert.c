/// @file assert.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "assert.h"
#include "stdio.h"
#include "stdlib.h"

void __assert_fail(const char *assertion, const char *file, const char *function, unsigned int line)
{
    printf(
        "\n=== ASSERTION FAILED ===\n"
        "Assertion: %s\n"
        "Location : %s:%d\n"
        "Function : %s\n\n",
        assertion, file, line, (function ? function : "Unknown function"));
    abort();
}
