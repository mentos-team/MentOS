/// @file test_utils.h
/// @brief Utility functions and macros for non-destructive kernel testing.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"
#include "stdint.h"

/// @defgroup TestUtilities Test Utilities
/// @brief Utilities for safe, non-destructive kernel testing during boot.
/// @{

/// @brief Mark the start of a critical test section (for test documentation).
/// @param description A description of what is being tested.
#define TEST_SECTION_START(description)            \
    do {                                           \
        pr_notice("  Testing: %s\n", description); \
    } while (0)

/// @brief Mark the end of a test section.
#define TEST_SECTION_END()                      \
    do {                                        \
        pr_notice("  ✓ Test section passed\n"); \
    } while (0)

/// @brief Assert and provide context about what failed.
/// @param cond The condition to check.
/// @param msg The message to display if condition fails.
#define ASSERT_MSG(cond, msg)                                                      \
    if (!(cond)) {                                                                 \
        pr_emerg("ASSERT failed in %s at line %d: %s\n", __func__, __LINE__, msg); \
        pr_emerg("Condition: %s\n", #cond);                                        \
        kernel_panic("Test failure");                                              \
    }

/// @brief Compare two memory regions and verify they're equal.
/// @param ptr1 First memory region.
/// @param ptr2 Second memory region.
/// @param size Size of the regions.
/// @param description Description of what is being compared.
/// @return 1 if equal, 0 if different.
static inline int test_memcmp(const void *ptr1, const void *ptr2, size_t size, const char *description)
{
    const unsigned char *p1 = (const unsigned char *)ptr1;
    const unsigned char *p2 = (const unsigned char *)ptr2;

    for (size_t i = 0; i < size; i++) {
        if (p1[i] != p2[i]) {
            pr_warning("Memcmp failed for %s at offset %zu: %02x != %02x\n", description, i, p1[i], p2[i]);
            return 0;
        }
    }
    return 1;
}

/// @brief Verify a memory range contains all zeros.
/// @param ptr The memory region to check.
/// @param size Size of the region.
/// @param description Description of what is being checked.
/// @return 1 if all zeros, 0 otherwise.
static inline int test_is_zeroed(const void *ptr, size_t size, const char *description)
{
    const unsigned char *p = (const unsigned char *)ptr;
    for (size_t i = 0; i < size; i++) {
        if (p[i] != 0) {
            pr_warning("Expected zero at offset %zu in %s, got %02x\n", i, description, p[i]);
            return 0;
        }
    }
    return 1;
}

/// @brief Verify a value is within expected bounds.
/// @param value The value to check.
/// @param min Minimum expected value (inclusive).
/// @param max Maximum expected value (inclusive).
/// @param description Description of what is being checked.
/// @return 1 if within bounds, 0 otherwise.
static inline int test_bounds_check(uint32_t value, uint32_t min, uint32_t max, const char *description)
{
    if (value < min || value > max) {
        pr_warning("Bounds check failed for %s: %u not in range [%u, %u]\n", description, value, min, max);
        return 0;
    }
    return 1;
}

/// @}
