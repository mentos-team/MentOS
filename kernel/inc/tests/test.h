/// @file test.h
/// @brief Kernel test framework macros and utilities.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "io/debug.h"  // For pr_emerg

/// @brief Define a test function.
/// @param name The name of the test.
#define TEST(name) void test_##name(void)

/// @brief Records one failed assertion against the running test run.
void kernel_test_record_failure(void);

/// @brief Returns how many assertions have failed so far.
/// @return The number of recorded failures.
unsigned kernel_test_failures(void);

/// @brief Assert a condition in tests.
/// @param cond The condition to check.
/// @details A failed assertion is recorded and the enclosing function returns,
/// so the remaining tests still run and the runner reports every failure at
/// the end. Only usable in functions returning void.
#define ASSERT(cond) \
    if (!(cond)) { \
        pr_emerg("ASSERT failed in %s: %s\n", __func__, #cond); \
        kernel_test_record_failure(); \
        return; \
    }
