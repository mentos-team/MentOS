///                MentOS, The Mentoring Operating system project
/// @file stdarg.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief The va_list type is an array containing a single element of one
///        structure containing the necessary information to implement the
///        va_arg macro.
typedef char *va_list;

/// @brief The type of the item on the stack.
#define va_item int

/// @brief Amount of space required in an argument list (ie. the stack) for an
/// argument of type t.
#define va_size(t)                                                             \
    (((sizeof(t) + sizeof(va_item) - 1) / sizeof(va_item)) * sizeof(va_item))

/// @brief The start of a variadic list.
#define va_start(ap, last_arg) (ap = ((va_list)(&last_arg) + va_size(last_arg)))
/// @brief The end of a variadic list.
#define va_end(ap) ((void)0)

/// @brief The argument of a variadic list.
#define va_arg(ap, t) (ap += va_size(t), *((t *)(ap - va_size(t))))
