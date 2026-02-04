/// @file test_buddy.c
/// @brief Buddy system internal tests.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "mem/alloc/zone_allocator.h"
#include "mem/gfp.h"
#include "mem/mm/page.h"
#include "tests/test.h"
#include "tests/test_utils.h"

/// @brief Test different order allocations (0 through 3).
TEST(memory_buddy_order_allocations)
{
    TEST_SECTION_START("Buddy order allocations");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *order0 = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(order0 != NULL, "order 0 allocation (1 page) must succeed");

    page_t *order1 = alloc_pages(GFP_KERNEL, 1);
    ASSERT_MSG(order1 != NULL, "order 1 allocation (2 pages) must succeed");

    page_t *order2 = alloc_pages(GFP_KERNEL, 2);
    ASSERT_MSG(order2 != NULL, "order 2 allocation (4 pages) must succeed");

    page_t *order3 = alloc_pages(GFP_KERNEL, 3);
    ASSERT_MSG(order3 != NULL, "order 3 allocation (8 pages) must succeed");

    ASSERT_MSG(free_pages(order3) == 0, "free order 3 must succeed");
    ASSERT_MSG(free_pages(order2) == 0, "free order 2 must succeed");
    ASSERT_MSG(free_pages(order1) == 0, "free order 1 must succeed");
    ASSERT_MSG(free_pages(order0) == 0, "free order 0 must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test that higher order allocations consume more memory.
TEST(memory_buddy_order_size_verification)
{
    TEST_SECTION_START("Buddy order size verification");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *order0             = alloc_pages(GFP_KERNEL, 0);
    unsigned long after_order0 = get_zone_free_space(GFP_KERNEL);
    uint32_t used_order0       = free_before - after_order0;

    ASSERT_MSG(free_pages(order0) == 0, "free order 0 must succeed");
    unsigned long restored = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(restored >= free_before, "Free space must be restored after order 0");

    page_t *order1             = alloc_pages(GFP_KERNEL, 1);
    unsigned long after_order1 = get_zone_free_space(GFP_KERNEL);
    uint32_t used_order1       = free_before - after_order1;

    ASSERT_MSG(used_order1 >= (used_order0 * 2), "order 1 must consume at least 2x order 0 space");

    ASSERT_MSG(free_pages(order1) == 0, "free order 1 must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test buddy coalescing by allocating and freeing in specific order.
TEST(memory_buddy_coalescing)
{
    TEST_SECTION_START("Buddy coalescing");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *pages[8];
    for (int i = 0; i < 8; ++i) {
        pages[i] = alloc_pages(GFP_KERNEL, 0);
        ASSERT_MSG(pages[i] != NULL, "allocation must succeed");
    }

    for (int i = 0; i < 8; ++i) {
        ASSERT_MSG(free_pages(pages[i]) == 0, "free must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Buddies must coalesce to restore free space");

    TEST_SECTION_END();
}

/// @brief Test split and merge cycles for order 2.
TEST(memory_buddy_split_merge)
{
    TEST_SECTION_START("Buddy split/merge");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *order2 = alloc_pages(GFP_KERNEL, 2);
    ASSERT_MSG(order2 != NULL, "order 2 allocation must succeed");

    ASSERT_MSG(free_pages(order2) == 0, "free order 2 must succeed");

    page_t *order0_a = alloc_pages(GFP_KERNEL, 0);
    page_t *order0_b = alloc_pages(GFP_KERNEL, 0);
    page_t *order0_c = alloc_pages(GFP_KERNEL, 0);
    page_t *order0_d = alloc_pages(GFP_KERNEL, 0);

    ASSERT_MSG(order0_a != NULL && order0_b != NULL && order0_c != NULL && order0_d != NULL, "4 order-0 allocations must succeed");

    ASSERT_MSG(free_pages(order0_a) == 0, "free must succeed");
    ASSERT_MSG(free_pages(order0_b) == 0, "free must succeed");
    ASSERT_MSG(free_pages(order0_c) == 0, "free must succeed");
    ASSERT_MSG(free_pages(order0_d) == 0, "free must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored after split/merge cycle");

    TEST_SECTION_END();
}

/// @brief Test allocation stress with mixed orders.
TEST(memory_buddy_mixed_order_stress)
{
    TEST_SECTION_START("Buddy mixed order stress");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    const unsigned int count = 16;
    page_t *allocs[count];

    for (unsigned int i = 0; i < count; ++i) {
        unsigned int order = i % 4;
        allocs[i]          = alloc_pages(GFP_KERNEL, order);
        ASSERT_MSG(allocs[i] != NULL, "allocation must succeed");
    }

    for (unsigned int i = 0; i < count; ++i) {
        ASSERT_MSG(free_pages(allocs[i]) == 0, "free must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test non-sequential free pattern (free even indices, then odd).
TEST(memory_buddy_non_sequential_free)
{
    TEST_SECTION_START("Buddy non-sequential free");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    const unsigned int count = 16;
    page_t *allocs[count];

    for (unsigned int i = 0; i < count; ++i) {
        allocs[i] = alloc_pages(GFP_KERNEL, 0);
        ASSERT_MSG(allocs[i] != NULL, "allocation must succeed");
    }

    for (unsigned int i = 0; i < count; i += 2) {
        ASSERT_MSG(free_pages(allocs[i]) == 0, "free even must succeed");
    }

    for (unsigned int i = 1; i < count; i += 2) {
        ASSERT_MSG(free_pages(allocs[i]) == 0, "free odd must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test large order allocation (if supported).
TEST(memory_buddy_large_order)
{
    TEST_SECTION_START("Buddy large order");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);
    unsigned long total_space = get_zone_total_space(GFP_KERNEL);

    if (total_space >= (1UL << 20)) {
        page_t *order6 = alloc_pages(GFP_KERNEL, 6);
        if (order6 != NULL) {
            ASSERT_MSG(free_pages(order6) == 0, "free large order must succeed");

            unsigned long free_after = get_zone_free_space(GFP_KERNEL);
            ASSERT_MSG(free_after >= free_before, "Free space must be restored");
        }
    }

    TEST_SECTION_END();
}

/// @brief Main test function for buddy system.
void test_buddy(void)
{
    test_memory_buddy_order_allocations();
    test_memory_buddy_order_size_verification();
    test_memory_buddy_coalescing();
    test_memory_buddy_split_merge();
    test_memory_buddy_mixed_order_stress();
    test_memory_buddy_non_sequential_free();
    test_memory_buddy_large_order();
}
