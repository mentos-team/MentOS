///                MentOS, The Mentoring Operating system project
/// @file   compiler.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/*
 * Prevent the compiler from merging or refetching reads or writes.
 *
 * Ensuring that the compiler does not fold, spindle, or otherwise
 * mutilate accesses that either do not require ordering or that interact
 * with an explicit memory barrier or atomic instruction that provides the
 * required ordering.
 */

/// @brief Assign the value to the given variable.
#define WRITE_ONCE(var, val) (*((volatile typeof(val) *)(&(var))) = (val))

/// @brief Read the value from the given variable.
#define READ_ONCE(var) (*((volatile typeof(var) *)(&(var))))
