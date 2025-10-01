/// @file test.h
/// @brief Kernel test framework macros and utilities.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "io/debug.h"  // For pr_emerg
#include "system/panic.h"  // For kernel_panic

/// @brief Define a test function.
/// @param name The name of the test.
#define TEST(name) void test_##name(void)

/// @brief Assert a condition in tests.
/// @param cond The condition to check.
#define ASSERT(cond) \
    if (!(cond)) { \
        pr_emerg("ASSERT failed in %s: %s\n", __func__, #cond); \
        kernel_panic("Test failure"); \
    }
