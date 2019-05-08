///                MentOS, The Mentoring Operating system project
/// @file   assert.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdio.h"
#include "panic.h"
#include "stdarg.h"

/// @brief           Function used to log the information of a failed assertion.
/// @param assertion The failed assertion.
/// @param file      The file where the assertion is located.
/// @param line      The line inside the file.
/// @param function  The function where the assertion is.
static void __assert_fail(const char *assertion, const char *file,
						  unsigned int line, const char *function)
{
	char message[1024];
	sprintf(message,
			"FILE: %s\n"
			"LINE: %d\n"
			"FUNC: %s\n\n"
			"Assertion `%s` failed.\n",
			file, line, (function ? function : "NO_FUN"), assertion);
	kernel_panic(message);
}

/// @brief Assert function.
#define assert(expression)                                                     \
	((expression) ? (void)0 :                                                  \
					__assert_fail(#expression, __FILE__, __LINE__, __func__))
