/// @file errno.c
/// @brief Stores the error number.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/errno.h"

/// @brief Returns the error number for the current process.
/// @return Pointer to the error number.
int *__geterrno()
{
    static int _errno = 0;
    return &_errno;
}
