/// @file assert.h
/// @brief Defines the function and pre-processor macro for assertions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "os_root_path.h"

/// @brief           Function used to log the information of a failed assertion.
/// @param assertion The failed assertion.
/// @param file      The file where the assertion is located.
/// @param function  The function where the assertion is.
/// @param line      The line inside the file.
void __assert_fail(const char *assertion, const char *file, const char *function, unsigned int line);

/// @brief Assert function.
#define assert(expression) ((expression) ? (void)0 : __assert_fail(#expression, __RELATIVE_PATH__, __func__, __LINE__))
