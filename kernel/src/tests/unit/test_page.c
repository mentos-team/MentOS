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

/// @brief Test HighMem pages have no permanent virtual address.
TEST(memory_page_highmem_no_virt)
{
    TEST_SECTION_START("HighMem page has no virtual mapping");

    if (memory.high_mem.size > 0) {
        page_t *page = get_page_from_physical_address(memory.high_mem.start_addr);
        ASSERT_MSG(page != NULL, "HighMem page must be resolvable from physical address");

        uint32_t vaddr = get_virtual_address_from_page(page);
        ASSERT_MSG(vaddr == 0, "HighMem page must not have a permanent virtual mapping");
    }

    TEST_SECTION_END();
}

/// @brief Test DMA pages map to DMA virtual range.
TEST(memory_page_dma_virt_range)
{
    TEST_SECTION_START("DMA page virtual range");

    if (memory.dma_mem.size > 0) {
        page_t *page = get_page_from_physical_address(memory.dma_mem.start_addr);
        ASSERT_MSG(page != NULL, "DMA page must be resolvable from physical address");

        uint32_t vaddr = get_virtual_address_from_page(page);
        ASSERT_MSG(vaddr >= memory.dma_mem.virt_start && vaddr < memory.dma_mem.virt_end,
                   "DMA page virtual address must be in DMA range");
    }

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

/// @brief Test LowMem virtual-physical offset consistency.
TEST(memory_page_lowmem_offset)
{
    TEST_SECTION_START("LowMem virt/phys offset");

    uint32_t phys = memory.low_mem.start_addr;
    page_t *page = get_page_from_physical_address(phys);
    ASSERT_MSG(page != NULL, "LowMem start page must be resolvable");

    uint32_t vaddr = get_virtual_address_from_page(page);
    uint32_t expected = memory.low_mem.virt_start - memory.low_mem.start_addr;
    ASSERT_MSG(vaddr - phys == expected, "LowMem virtual-physical offset must match");

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

/// @brief Test that HighMem pages require kmap for virtual access.
TEST(memory_page_highmem_requires_kmap)
{
    TEST_SECTION_START("HighMem requires kmap");

    // Try to allocate a HighMem page
    page_t *highmem_page = alloc_pages(GFP_HIGHUSER, 0);

    if (highmem_page != NULL && is_highmem_page_struct(highmem_page)) {
        // HighMem page allocated - kmap would be required in real code
        // For this test, just verify get_virtual_address_from_page returns 0
        uint32_t virt = get_virtual_address_from_page(highmem_page);
        ASSERT_MSG(virt == 0, "HighMem page virt address must be 0 (requires kmap)");

        ASSERT_MSG(free_pages(highmem_page) == 0, "free_pages must succeed");
    }
    // If no HighMem available, test still passes

    TEST_SECTION_END();
}

/// @brief Test that get_page_from_virtual_address rejects HighMem ranges.
TEST(memory_page_virt_address_rejects_highmem)
{
    TEST_SECTION_START("get_page_from_virtual_address rejects HighMem");

    // Try to get a page from a HighMem virtual address
    // HighMem doesn't have permanent virtual mappings, so asking for a page
    // from a random high address should return NULL

    unsigned long total_high = get_zone_total_space(GFP_HIGHUSER);
    if (total_high > 0) {
        // HighMem exists - try to translate a bogus high virtual address
        // This should return NULL or 0 since we're not using kmap
        uint32_t bogus_highmem_addr = memory.high_mem.virt_start;
        page_t *page = get_page_from_virtual_address(bogus_highmem_addr);
        
        // The function should return NULL for unmapped HighMem regions
        // (get_page_from_virtual_address only works for lowmem)
        if (page != NULL) {
            // If it did return something, it should not be from HighMem
            ASSERT_MSG(!is_highmem_page_struct(page), "Page must not be from HighMem for unmapped virtual");
        }
    }

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
    test_memory_page_highmem_no_virt();
    test_memory_page_dma_virt_range();
    test_memory_page_get_phys_addr();
    test_memory_page_virt_phys_relationship();
    test_memory_page_lowmem_offset();
    test_memory_page_write_read_virt();
    test_memory_page_highmem_requires_kmap();
    test_memory_page_virt_address_rejects_highmem();
}
