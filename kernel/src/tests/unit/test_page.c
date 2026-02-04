/// @file test_page.c
/// @brief Page structure and reference counting tests.
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
#include "mem/paging.h"
#include "tests/test.h"
#include "tests/test_utils.h"

/// @brief Test page structure size and alignment.
TEST(memory_page_structure_size)
{
    TEST_SECTION_START("Page structure size");

    ASSERT_MSG(sizeof(page_t) > 0, "page_t must have non-zero size");
    ASSERT_MSG(sizeof(atomic_t) == 4, "atomic_t must be 4 bytes");

    TEST_SECTION_END();
}

/// @brief Test page reference counter initialization.
TEST(memory_page_count_init)
{
    TEST_SECTION_START("Page count initialization");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must succeed");

    int count = page_count(page);
    ASSERT_MSG(count > 0, "page count must be positive after allocation");

    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test page_inc and page_dec operations.
TEST(memory_page_inc_dec)
{
    TEST_SECTION_START("Page inc/dec");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must succeed");

    int count_before = page_count(page);

    page_inc(page);
    int count_after_inc = page_count(page);
    ASSERT_MSG(count_after_inc == count_before + 1, "page_inc must increment count");

    page_dec(page);
    int count_after_dec = page_count(page);
    ASSERT_MSG(count_after_dec == count_before, "page_dec must decrement count");

    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test set_page_count operation.
TEST(memory_page_set_count)
{
    TEST_SECTION_START("Page set count");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must succeed");

    set_page_count(page, 5);
    int count = page_count(page);
    ASSERT_MSG(count == 5, "set_page_count must set count to specified value");

    set_page_count(page, 1);
    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test get_virtual_address_from_page.
TEST(memory_page_get_virt_addr)
{
    TEST_SECTION_START("Page get virtual address");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must succeed");

    uint32_t vaddr = get_virtual_address_from_page(page);
    ASSERT_MSG(vaddr != 0, "get_virtual_address_from_page must return non-zero");
    ASSERT_MSG(vaddr >= PROCAREA_END_ADDR, "lowmem virtual address must be in kernel space");
    ASSERT_MSG((vaddr & (PAGE_SIZE - 1)) == 0, "virtual address must be page-aligned");

    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test get_physical_address_from_page.
TEST(memory_page_get_phys_addr)
{
    TEST_SECTION_START("Page get physical address");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must succeed");

    uint32_t paddr = get_physical_address_from_page(page);
    ASSERT_MSG(paddr != 0, "get_physical_address_from_page must return non-zero");
    ASSERT_MSG((paddr & (PAGE_SIZE - 1)) == 0, "physical address must be page-aligned");

    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test virtual-physical address relationship for lowmem.
TEST(memory_page_virt_phys_relationship)
{
    TEST_SECTION_START("Page virt/phys relationship");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must succeed");

    uint32_t vaddr = get_virtual_address_from_page(page);
    uint32_t paddr = get_physical_address_from_page(page);

    ASSERT_MSG(vaddr > paddr, "lowmem virtual address must be higher than physical");
    ASSERT_MSG(vaddr >= PROCAREA_END_ADDR, "lowmem virtual address must be in kernel space");

    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test page write/read through virtual address.
TEST(memory_page_write_read_virt)
{
    TEST_SECTION_START("Page write/read via virtual address");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must succeed");

    uint32_t vaddr = get_virtual_address_from_page(page);
    uint8_t *ptr   = (uint8_t *)vaddr;

    for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
        ptr[i] = (uint8_t)(0xAA ^ i);
    }

    for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
        ASSERT_MSG(ptr[i] == (uint8_t)(0xAA ^ i), "page data must persist");
    }

    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free space must be restored");

    TEST_SECTION_END();
}

/// @brief Main test function for page structure.
void test_page(void)
{
    test_memory_page_structure_size();
    test_memory_page_count_init();
    test_memory_page_inc_dec();
    test_memory_page_set_count();
    test_memory_page_get_virt_addr();
    test_memory_page_get_phys_addr();
    test_memory_page_virt_phys_relationship();
    test_memory_page_write_read_virt();
}
