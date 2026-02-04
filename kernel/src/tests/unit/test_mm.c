/// @file test_mm.c
/// @brief mm/vm_area tests.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "mem/gfp.h"
#include "mem/mm/mm.h"
#include "mem/mm/vm_area.h"
#include "mem/paging.h"
#include "tests/test.h"
#include "tests/test_utils.h"

/// @brief Test mm and vm_area lifecycle.
TEST(memory_mm_vm_area_lifecycle)
{
    TEST_SECTION_START("MM/VMA lifecycle");

    mm_struct_t *mm = mm_create_blank(PAGE_SIZE * 2);
    ASSERT_MSG(mm != NULL, "mm_create_blank must succeed");
    ASSERT_MSG(mm->pgd != NULL, "mm->pgd must be initialized");
    ASSERT_MSG(mm->map_count >= 1, "mm->map_count must be >= 1");

    ASSERT_MSG(mm->mmap_cache != NULL, "mm->mmap_cache must be initialized");

    vm_area_struct_t *stack = vm_area_find(mm, mm->start_stack);
    ASSERT_MSG(stack != NULL, "stack VMA must be discoverable");

    uintptr_t vm_start = 0;
    int search_rc      = vm_area_search_free_area(mm, PAGE_SIZE, &vm_start);
    ASSERT_MSG(search_rc == 0 || search_rc == 1, "vm_area_search_free_area must return 0 or 1");

    if (search_rc == 0) {
        vm_area_struct_t *segment =
            vm_area_create(mm, (uint32_t)vm_start, PAGE_SIZE, MM_PRESENT | MM_RW | MM_USER, GFP_HIGHUSER);
        ASSERT_MSG(segment != NULL, "vm_area_create must succeed");

        ASSERT_MSG(vm_area_find(mm, (uint32_t)vm_start) == segment, "vm_area_find must locate the segment");
        ASSERT_MSG(vm_area_destroy(mm, segment) == 0, "vm_area_destroy must succeed");
    }

    ASSERT_MSG(mm_destroy(mm) == 0, "mm_destroy must succeed");

    TEST_SECTION_END();
}

/// @brief Main test function for mm subsystem.
void test_mm(void)
{
    test_memory_mm_vm_area_lifecycle();
}
