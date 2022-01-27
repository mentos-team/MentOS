/// @file strerror.h
/// @brief Contains the function that transfornms an errno into a string.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include <sys/errno.h>

/// @brief Returns the string representing the error number.
/// @param errnum The error number.
/// @return The string representing the error number.
char *strerror(int errnum);
