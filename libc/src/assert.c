/// @file assert.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "assert.h"
#include "stdio.h"
#include "stdlib.h"

void __assert_fail(const char *assertion, const char *file, const char *function, unsigned int line)
{
    printf("FILE: %s\n"
           "FUNC: %s\n"
           "LINE: %d\n\n"
           "Assertion `%s` failed.\n",
           file, (function ? function : "NO_FUN"), line, assertion);
    abort();
}
