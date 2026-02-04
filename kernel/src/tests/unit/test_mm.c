/// @file test_mm.c
/// @brief mm/vm_area tests.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "mem/alloc/zone_allocator.h"
#include "mem/gfp.h"
#include "mem/mm/mm.h"
#include "mem/mm/page.h"
#include "mem/mm/vm_area.h"
#include "mem/paging.h"
#include "tests/test.h"
#include "tests/test_utils.h"

static uint32_t mm_test_rand(uint32_t *state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

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

/// @brief Test basic properties of a freshly created mm.
TEST(memory_mm_create_blank_sanity)
{
    TEST_SECTION_START("MM create blank sanity");

    size_t stack_size = PAGE_SIZE * 2;
    mm_struct_t *mm   = mm_create_blank(stack_size);
    ASSERT_MSG(mm != NULL, "mm_create_blank must succeed");
    ASSERT_MSG(mm->pgd != NULL, "mm->pgd must be initialized");
    ASSERT_MSG(mm->start_stack == (PROCAREA_END_ADDR - stack_size), "start_stack must match requested size");
    ASSERT_MSG(mm->map_count >= 1, "map_count must be >= 1");

    ASSERT_MSG(mm_destroy(mm) == 0, "mm_destroy must succeed");

    TEST_SECTION_END();
}

/// @brief Test cloning of mm structures.
TEST(memory_mm_clone)
{
    TEST_SECTION_START("MM clone");

    mm_struct_t *mm = mm_create_blank(PAGE_SIZE * 2);
    ASSERT_MSG(mm != NULL, "mm_create_blank must succeed");

    mm_struct_t *clone = mm_clone(mm);
    ASSERT_MSG(clone != NULL, "mm_clone must succeed");
    ASSERT_MSG(clone->pgd != NULL, "clone->pgd must be initialized");
    ASSERT_MSG(clone->pgd != mm->pgd, "clone must have a distinct page directory");
    ASSERT_MSG(clone->map_count == mm->map_count, "clone must preserve map_count");

    ASSERT_MSG(mm_destroy(clone) == 0, "mm_destroy(clone) must succeed");
    ASSERT_MSG(mm_destroy(mm) == 0, "mm_destroy(mm) must succeed");

    TEST_SECTION_END();
}

/// @brief Test cloned mm gets separate physical pages for present mappings.
TEST(memory_mm_clone_separate_pages)
{
    TEST_SECTION_START("MM clone separate pages");

    mm_struct_t *mm = mm_create_blank(PAGE_SIZE * 2);
    ASSERT_MSG(mm != NULL, "mm_create_blank must succeed");

    uintptr_t vm_start = 0;
    int search_rc      = vm_area_search_free_area(mm, PAGE_SIZE, &vm_start);
    ASSERT_MSG(search_rc == 0 || search_rc == 1, "vm_area_search_free_area must return 0 or 1");

    if (search_rc == 0) {
        vm_area_struct_t *segment =
            vm_area_create(mm, (uint32_t)vm_start, PAGE_SIZE, MM_PRESENT | MM_RW | MM_USER, GFP_HIGHUSER);
        ASSERT_MSG(segment != NULL, "vm_area_create must succeed");

        mm_struct_t *clone = mm_clone(mm);
        ASSERT_MSG(clone != NULL, "mm_clone must succeed");

        size_t size_a = PAGE_SIZE;
        size_t size_b = PAGE_SIZE;
        page_t *page_a = mem_virtual_to_page(mm->pgd, (uint32_t)vm_start, &size_a);
        page_t *page_b = mem_virtual_to_page(clone->pgd, (uint32_t)vm_start, &size_b);

        ASSERT_MSG(page_a != NULL && page_b != NULL, "both mappings must be present");
        ASSERT_MSG(page_a != page_b, "clone must not share physical pages for present mapping");

        ASSERT_MSG(mm_destroy(clone) == 0, "mm_destroy(clone) must succeed");
    }

    ASSERT_MSG(mm_destroy(mm) == 0, "mm_destroy(mm) must succeed");

    TEST_SECTION_END();
}

/// @brief Test cloned mm copies page contents for present mappings.
TEST(memory_mm_clone_copies_content)
{
    TEST_SECTION_START("MM clone copies content");

    mm_struct_t *mm = mm_create_blank(PAGE_SIZE * 2);
    ASSERT_MSG(mm != NULL, "mm_create_blank must succeed");

    uintptr_t vm_start = 0;
    int search_rc      = vm_area_search_free_area(mm, PAGE_SIZE, &vm_start);
    ASSERT_MSG(search_rc == 0 || search_rc == 1, "vm_area_search_free_area must return 0 or 1");

    if (search_rc == 0) {
        vm_area_struct_t *segment =
            vm_area_create(mm, (uint32_t)vm_start, PAGE_SIZE, MM_PRESENT | MM_RW | MM_USER, GFP_HIGHUSER);
        ASSERT_MSG(segment != NULL, "vm_area_create must succeed");

        size_t size_a = PAGE_SIZE;
        page_t *page_a = mem_virtual_to_page(mm->pgd, (uint32_t)vm_start, &size_a);
        ASSERT_MSG(page_a != NULL, "source mapping must be present");

        uint32_t lowmem_a = get_virtual_address_from_page(page_a);
        ASSERT_MSG(lowmem_a != 0, "get_virtual_address_from_page must succeed");

        uint8_t *ptr_a = (uint8_t *)lowmem_a;
        for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
            ptr_a[i] = (uint8_t)(0x7B ^ i);
        }

        mm_struct_t *clone = mm_clone(mm);
        ASSERT_MSG(clone != NULL, "mm_clone must succeed");

        size_t size_b = PAGE_SIZE;
        page_t *page_b = mem_virtual_to_page(clone->pgd, (uint32_t)vm_start, &size_b);
        ASSERT_MSG(page_b != NULL, "clone mapping must be present");
        ASSERT_MSG(page_a != page_b, "clone must not share physical pages");

        uint32_t lowmem_b = get_virtual_address_from_page(page_b);
        ASSERT_MSG(lowmem_b != 0, "get_virtual_address_from_page must succeed");

        uint8_t *ptr_b = (uint8_t *)lowmem_b;
        for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
            ASSERT_MSG(ptr_b[i] == (uint8_t)(0x7B ^ i), "clone must preserve content");
        }

        ASSERT_MSG(mm_destroy(clone) == 0, "mm_destroy(clone) must succeed");
    }

    ASSERT_MSG(mm_destroy(mm) == 0, "mm_destroy(mm) must succeed");

    TEST_SECTION_END();
}

/// @brief Test cloned mm copies content across multiple pages.
TEST(memory_mm_clone_copies_multi_page)
{
    TEST_SECTION_START("MM clone copies multi-page");

    const uint32_t pages = 3;
    const uint32_t size  = pages * PAGE_SIZE;

    mm_struct_t *mm = mm_create_blank(PAGE_SIZE * 2);
    ASSERT_MSG(mm != NULL, "mm_create_blank must succeed");

    uintptr_t vm_start = 0;
    int search_rc      = vm_area_search_free_area(mm, size, &vm_start);
    ASSERT_MSG(search_rc == 0 || search_rc == 1, "vm_area_search_free_area must return 0 or 1");

    if (search_rc == 0) {
        vm_area_struct_t *segment =
            vm_area_create(mm, (uint32_t)vm_start, size, MM_PRESENT | MM_RW | MM_USER, GFP_HIGHUSER);
        ASSERT_MSG(segment != NULL, "vm_area_create must succeed");

        for (uint32_t p = 0; p < pages; ++p) {
            uint32_t addr = (uint32_t)vm_start + (p * PAGE_SIZE);
            size_t size_a = PAGE_SIZE;
            page_t *page_a = mem_virtual_to_page(mm->pgd, addr, &size_a);
            ASSERT_MSG(page_a != NULL, "source mapping must be present");

            uint32_t lowmem_a = get_virtual_address_from_page(page_a);
            ASSERT_MSG(lowmem_a != 0, "get_virtual_address_from_page must succeed");

            uint8_t *ptr_a = (uint8_t *)lowmem_a;
            for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
                ptr_a[i] = (uint8_t)(0xA3 ^ i ^ p);
            }
        }

        mm_struct_t *clone = mm_clone(mm);
        ASSERT_MSG(clone != NULL, "mm_clone must succeed");

        for (uint32_t p = 0; p < pages; ++p) {
            uint32_t addr = (uint32_t)vm_start + (p * PAGE_SIZE);
            size_t size_b = PAGE_SIZE;
            page_t *page_b = mem_virtual_to_page(clone->pgd, addr, &size_b);
            ASSERT_MSG(page_b != NULL, "clone mapping must be present");

            uint32_t lowmem_b = get_virtual_address_from_page(page_b);
            ASSERT_MSG(lowmem_b != 0, "get_virtual_address_from_page must succeed");

            uint8_t *ptr_b = (uint8_t *)lowmem_b;
            for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
                ASSERT_MSG(ptr_b[i] == (uint8_t)(0xA3 ^ i ^ p), "clone must preserve content");
            }
        }

        ASSERT_MSG(mm_destroy(clone) == 0, "mm_destroy(clone) must succeed");
    }

    ASSERT_MSG(mm_destroy(mm) == 0, "mm_destroy(mm) must succeed");

    TEST_SECTION_END();
}

/// @brief Stress mm create/clone/destroy to detect leaks.
TEST(memory_mm_lifecycle_stress)
{
    TEST_SECTION_START("MM lifecycle stress");

    const unsigned int rounds = 8;
    unsigned long base_low_free = 0;
    unsigned long base_high_free = 0;
    unsigned long total_high = get_zone_total_space(GFP_HIGHUSER);

    for (unsigned int r = 0; r < rounds; ++r) {
        mm_struct_t *mm = mm_create_blank(PAGE_SIZE * 2);
        ASSERT_MSG(mm != NULL, "mm_create_blank must succeed");

        mm_struct_t *clone = mm_clone(mm);
        ASSERT_MSG(clone != NULL, "mm_clone must succeed");

        ASSERT_MSG(mm_destroy(clone) == 0, "mm_destroy(clone) must succeed");
        ASSERT_MSG(mm_destroy(mm) == 0, "mm_destroy(mm) must succeed");

        unsigned long low_free = get_zone_free_space(GFP_KERNEL);
        unsigned long high_free = (total_high > 0) ? get_zone_free_space(GFP_HIGHUSER) : 0;

        if (r == 0) {
            base_low_free = low_free;
            base_high_free = high_free;
        } else {
            ASSERT_MSG(low_free >= base_low_free, "lowmem free space must not decrease after warmup");
            if (total_high > 0) {
                ASSERT_MSG(high_free >= base_high_free, "highmem free space must not decrease after warmup");
            }
        }
    }

    TEST_SECTION_END();
}

/// @brief Stress randomized VMA creation/destruction patterns.
TEST(memory_mm_vma_randomized)
{
    TEST_SECTION_START("MM VMA randomized");

    mm_struct_t *mm = mm_create_blank(PAGE_SIZE * 2);
    ASSERT_MSG(mm != NULL, "mm_create_blank must succeed");

    const unsigned int max_segments = 8;
    vm_area_struct_t *segments[max_segments];
    for (unsigned int i = 0; i < max_segments; ++i) {
        segments[i] = NULL;
    }

    unsigned int created = 0;
    uint32_t rng = 0xC0FFEEu;

    for (unsigned int i = 0; i < max_segments; ++i) {
        uint32_t pages = (mm_test_rand(&rng) % 4) + 1;
        size_t size = pages * PAGE_SIZE;

        uintptr_t vm_start = 0;
        int search_rc = vm_area_search_free_area(mm, size, &vm_start);
        if (search_rc != 0) {
            continue;
        }

        vm_area_struct_t *segment =
            vm_area_create(mm, (uint32_t)vm_start, size, MM_PRESENT | MM_RW | MM_USER, GFP_HIGHUSER);
        if (!segment) {
            continue;
        }

        segments[created++] = segment;
    }

    for (unsigned int i = 0; i < created; ++i) {
        unsigned int idx = (mm_test_rand(&rng) % created);
        if (segments[idx]) {
            ASSERT_MSG(vm_area_destroy(mm, segments[idx]) == 0, "vm_area_destroy must succeed");
            segments[idx] = NULL;
        }
    }

    ASSERT_MSG(mm_destroy(mm) == 0, "mm_destroy must succeed");

    TEST_SECTION_END();
}

/// @brief Fragmentation-like VMA pattern with non-sequential frees.
TEST(memory_mm_vma_fragmentation)
{
    TEST_SECTION_START("MM VMA fragmentation");

    mm_struct_t *mm = mm_create_blank(PAGE_SIZE * 2);
    ASSERT_MSG(mm != NULL, "mm_create_blank must succeed");

    const unsigned int count = 6;
    vm_area_struct_t *segments[count];
    for (unsigned int i = 0; i < count; ++i) {
        segments[i] = NULL;
    }

    for (unsigned int i = 0; i < count; ++i) {
        size_t size = ((i % 2) + 1) * PAGE_SIZE;
        uintptr_t vm_start = 0;
        int search_rc = vm_area_search_free_area(mm, size, &vm_start);
        if (search_rc != 0) {
            continue;
        }

        segments[i] =
            vm_area_create(mm, (uint32_t)vm_start, size, MM_PRESENT | MM_RW | MM_USER, GFP_HIGHUSER);
    }

    for (unsigned int i = 0; i < count; i += 2) {
        if (segments[i]) {
            ASSERT_MSG(vm_area_destroy(mm, segments[i]) == 0, "vm_area_destroy must succeed");
            segments[i] = NULL;
        }
    }
    for (unsigned int i = 1; i < count; i += 2) {
        if (segments[i]) {
            ASSERT_MSG(vm_area_destroy(mm, segments[i]) == 0, "vm_area_destroy must succeed");
            segments[i] = NULL;
        }
    }

    ASSERT_MSG(mm_destroy(mm) == 0, "mm_destroy must succeed");

    TEST_SECTION_END();
}

/// @brief Main test function for mm subsystem.
void test_mm(void)
{
    test_memory_mm_vm_area_lifecycle();
    test_memory_mm_create_blank_sanity();
    test_memory_mm_clone();
    test_memory_mm_clone_separate_pages();
    test_memory_mm_clone_copies_content();
    test_memory_mm_lifecycle_stress();
    test_memory_mm_clone_copies_multi_page();
    test_memory_mm_vma_randomized();
    test_memory_mm_vma_fragmentation();
}
