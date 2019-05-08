///                MentOS, The Mentoring Operating system project
/// @file assert.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "assert.h"
#include "stdio.h"
#include "panic.h"

void __assert_fail(const char *assertion,
                          const char *file,
                          unsigned int line,
                          const char *function)
{
    char message[1024];
    sprintf(message,
            "FILE: %s\n"
            "LINE: %d\n"
            "FUNC: %s\n\n"
            "Assertion `%s` failed.\n",
            file,
            line,
            (function ? function : "NO_FUN"),
            assertion);
    kernel_panic(message);
}
