/// @file test_zone_allocator.c
/// @brief Zone allocator and buddy system tests.
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

/// @brief Test that the memory info structure is initialized and consistent.
TEST(memory_info_integrity)
{
    TEST_SECTION_START("Memory info integrity");

    ASSERT_MSG(memory.mem_map != NULL, "mem_map must be initialized");
    ASSERT_MSG(memory.page_data != NULL, "page_data must be initialized");
    ASSERT_MSG(memory.mem_size > 0, "mem_size must be > 0");
    ASSERT_MSG(memory.mem_map_num > 0, "mem_map_num must be > 0");
    ASSERT_MSG(memory.page_index_min <= memory.page_index_max, "page index range must be valid");

    ASSERT_MSG(memory.low_mem.size > 0, "low_mem size must be > 0");
    ASSERT_MSG(memory.low_mem.start_addr < memory.low_mem.end_addr, "low_mem address range invalid");
    ASSERT_MSG(
        memory.low_mem.size == (memory.low_mem.end_addr - memory.low_mem.start_addr),
        "low_mem size must match range");
    ASSERT_MSG((memory.low_mem.start_addr & (PAGE_SIZE - 1)) == 0, "low_mem start must be page-aligned");
    ASSERT_MSG((memory.low_mem.end_addr & (PAGE_SIZE - 1)) == 0, "low_mem end must be page-aligned");
    ASSERT_MSG(memory.low_mem.virt_start < memory.low_mem.virt_end, "low_mem virtual range invalid");

    if (memory.high_mem.size > 0) {
        ASSERT_MSG(memory.high_mem.start_addr < memory.high_mem.end_addr, "high_mem address range invalid");
        ASSERT_MSG(
            memory.high_mem.size == (memory.high_mem.end_addr - memory.high_mem.start_addr),
            "high_mem size must match range");
        ASSERT_MSG((memory.high_mem.start_addr & (PAGE_SIZE - 1)) == 0, "high_mem start must be page-aligned");
        ASSERT_MSG((memory.high_mem.end_addr & (PAGE_SIZE - 1)) == 0, "high_mem end must be page-aligned");
        ASSERT_MSG(
            memory.high_mem.virt_end == (memory.high_mem.virt_start + memory.high_mem.size),
            "high_mem virtual range must match size");
    }

    ASSERT_MSG(
        memory.page_index_min == (memory.low_mem.start_addr / PAGE_SIZE),
        "page_index_min must match low_mem start PFN");

    TEST_SECTION_END();
}

/// @brief Test validity checks for virtual addresses.
TEST(memory_virtual_address_validation)
{
    TEST_SECTION_START("Virtual address validation");

    ASSERT_MSG(is_valid_virtual_address(memory.low_mem.virt_start) == 1, "low_mem start must be valid");

    if (memory.low_mem.virt_end > memory.low_mem.virt_start) {
        ASSERT_MSG(
            is_valid_virtual_address(memory.low_mem.virt_end - 1) == 1, "low_mem end-1 must be valid");
    }

    if (memory.low_mem.virt_start >= PAGE_SIZE) {
        ASSERT_MSG(
            is_valid_virtual_address(memory.low_mem.virt_start - PAGE_SIZE) == 0,
            "address below low_mem must be invalid");
    }

    unsigned long total_high = get_zone_total_space(GFP_HIGHUSER);
    if (total_high > 0 && memory.high_mem.virt_end > memory.high_mem.virt_start) {
        ASSERT_MSG(is_valid_virtual_address(memory.high_mem.virt_start) == 1, "high_mem start must be valid");
        ASSERT_MSG(
            is_valid_virtual_address(memory.high_mem.virt_end - 1) == 1, "high_mem end-1 must be valid");
        ASSERT_MSG(is_valid_virtual_address(memory.high_mem.virt_end) == 0, "high_mem end must be invalid");
    } else {
        ASSERT_MSG(
            is_valid_virtual_address(memory.low_mem.virt_end) == 0,
            "low_mem end must be invalid when no high_mem");
    }

    TEST_SECTION_END();
}

/// @brief Test order calculation for allocations.
TEST(memory_order_calculation)
{
    TEST_SECTION_START("Order calculation");

    ASSERT_MSG(find_nearest_order_greater(0, PAGE_SIZE) == 0, "1 page must be order 0");
    ASSERT_MSG(find_nearest_order_greater(0, PAGE_SIZE + 1) == 1, "2 pages must be order 1");
    ASSERT_MSG(find_nearest_order_greater(0, PAGE_SIZE * 2) == 1, "2 pages must be order 1");
    ASSERT_MSG(find_nearest_order_greater(0, PAGE_SIZE * 3) == 2, "3 pages must be order 2");
    ASSERT_MSG(find_nearest_order_greater(PAGE_SIZE * 5, PAGE_SIZE) == 0, "aligned single page must be order 0");

    TEST_SECTION_END();
}

/// @brief Test zone metrics and buddy status strings.
TEST(memory_zone_space_metrics)
{
    TEST_SECTION_START("Zone space metrics");

    unsigned long total  = get_zone_total_space(GFP_KERNEL);
    unsigned long free   = get_zone_free_space(GFP_KERNEL);
    unsigned long cached = get_zone_cached_space(GFP_KERNEL);

    ASSERT_MSG(total > 0, "GFP_KERNEL total space must be > 0");
    ASSERT_MSG(free <= total, "GFP_KERNEL free space must be <= total");
    ASSERT_MSG(cached <= total, "GFP_KERNEL cached space must be <= total");

    char buddy_status[256] = {0};
    int status_len         = get_zone_buddy_system_status(GFP_KERNEL, buddy_status, sizeof(buddy_status));
    ASSERT_MSG(status_len > 0, "Buddy system status must be non-empty");
    ASSERT_MSG(buddy_status[0] != '\0', "Buddy system status must contain data");

    unsigned long total_high = get_zone_total_space(GFP_HIGHUSER);
    if (total_high > 0) {
        unsigned long free_high   = get_zone_free_space(GFP_HIGHUSER);
        unsigned long cached_high = get_zone_cached_space(GFP_HIGHUSER);
        ASSERT_MSG(free_high <= total_high, "GFP_HIGHUSER free space must be <= total");
        ASSERT_MSG(cached_high <= total_high, "GFP_HIGHUSER cached space must be <= total");
    }

    TEST_SECTION_END();
}

/// @brief Test zone total sizes match configuration bounds.
TEST(memory_zone_total_space_matches)
{
    TEST_SECTION_START("Zone total space matches");

    unsigned long total_low = get_zone_total_space(GFP_KERNEL);
    ASSERT_MSG(total_low > 0, "Lowmem total space must be > 0");
    ASSERT_MSG(total_low <= memory.low_mem.size, "Lowmem total space must be within low_mem size");

    unsigned long total_high = get_zone_total_space(GFP_HIGHUSER);
    if (total_high > 0) {
        ASSERT_MSG(total_high <= memory.high_mem.size, "Highmem total space must be within high_mem size");
    }

    TEST_SECTION_END();
}

/// @brief Test single-page allocation and free in buddy system.
TEST(memory_alloc_free_roundtrip)
{
    TEST_SECTION_START("Alloc/free roundtrip");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must return a valid page");
    ASSERT_MSG(is_lowmem_page_struct(page), "GFP_KERNEL page must be in lowmem map");

    unsigned long free_after_alloc = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after_alloc < free_before, "free space must decrease after alloc");

    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    unsigned long free_after_free = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after_free >= free_before, "free space must be restored after free");

    TEST_SECTION_END();
}

/// @brief Test multi-page allocation and free in buddy system.
TEST(memory_alloc_free_order1)
{
    TEST_SECTION_START("Alloc/free order-1");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *page = alloc_pages(GFP_KERNEL, 1);
    ASSERT_MSG(page != NULL, "alloc_pages(order=1) must return a valid page");
    ASSERT_MSG(is_lowmem_page_struct(page), "GFP_KERNEL page must be in lowmem map");

    unsigned long free_after_alloc = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after_alloc < free_before, "free space must decrease after alloc");
    ASSERT_MSG((free_before - free_after_alloc) >= PAGE_SIZE, "free space delta must be at least one page");

    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    unsigned long free_after_free = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after_free >= free_before, "free space must be restored after free");

    TEST_SECTION_END();
}

/// @brief Stress alloc/free patterns to detect buddy leaks.
TEST(memory_alloc_free_stress)
{
    TEST_SECTION_START("Alloc/free stress");

    const unsigned int count = 32;
    page_t *pages[count];

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    for (unsigned int i = 0; i < count; ++i) {
        pages[i] = alloc_pages(GFP_KERNEL, 0);
        ASSERT_MSG(pages[i] != NULL, "alloc_pages must succeed");
    }

    for (unsigned int i = 0; i < count; ++i) {
        ASSERT_MSG(free_pages(pages[i]) == 0, "free_pages must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "free space must be restored after stress");

    TEST_SECTION_END();
}

/// @brief Fragmentation pattern should fully recover free space.
TEST(memory_alloc_free_fragmentation)
{
    TEST_SECTION_START("Alloc/free fragmentation");

    page_t *order0[8];
    page_t *order1[4];

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    for (unsigned int i = 0; i < 8; ++i) {
        order0[i] = alloc_pages(GFP_KERNEL, 0);
        ASSERT_MSG(order0[i] != NULL, "alloc_pages(order=0) must succeed");
    }
    for (unsigned int i = 0; i < 4; ++i) {
        order1[i] = alloc_pages(GFP_KERNEL, 1);
        ASSERT_MSG(order1[i] != NULL, "alloc_pages(order=1) must succeed");
    }

    for (unsigned int i = 0; i < 4; ++i) {
        ASSERT_MSG(free_pages(order1[i]) == 0, "free_pages(order=1) must succeed");
    }
    for (unsigned int i = 0; i < 8; ++i) {
        ASSERT_MSG(free_pages(order0[i]) == 0, "free_pages(order=0) must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "free space must be restored after fragmentation");

    TEST_SECTION_END();
}

/// @brief Test lowmem allocation helpers.
TEST(memory_lowmem_alloc_free)
{
    TEST_SECTION_START("Lowmem alloc/free");

    uint32_t vaddr = alloc_pages_lowmem(GFP_KERNEL, 0);
    ASSERT_MSG(vaddr != 0, "alloc_pages_lowmem must return a valid address");
    ASSERT_MSG(is_valid_virtual_address(vaddr) == 1, "lowmem address must be valid");
    ASSERT_MSG(free_pages_lowmem(vaddr) == 0, "free_pages_lowmem must succeed");

    TEST_SECTION_END();
}

/// @brief Test lowmem allocator rejects non-kernel GFP masks.
TEST(memory_lowmem_rejects_highuser)
{
    TEST_SECTION_START("Lowmem rejects highuser");

    uint32_t vaddr = alloc_pages_lowmem(GFP_HIGHUSER, 0);
    ASSERT_MSG(vaddr == 0, "alloc_pages_lowmem must reject GFP_HIGHUSER");

    TEST_SECTION_END();
}

/// @brief Test page <-> address conversion helpers.
TEST(memory_page_address_roundtrip)
{
    TEST_SECTION_START("Page/address roundtrip");

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must return a valid page");

    uint32_t vaddr = get_virtual_address_from_page(page);
    ASSERT_MSG(vaddr != 0, "get_virtual_address_from_page must succeed");
    ASSERT_MSG(get_page_from_virtual_address(vaddr) == page, "virtual address must map back to page");

    uint32_t paddr = get_physical_address_from_page(page);
    ASSERT_MSG(paddr != 0, "get_physical_address_from_page must succeed");
    ASSERT_MSG(get_page_from_physical_address(paddr) == page, "physical address must map back to page");

    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    TEST_SECTION_END();
}

/// @brief Test write/read on a freshly allocated page.
TEST(memory_page_write_read)
{
    TEST_SECTION_START("Page write/read");

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must return a valid page");

    uint32_t vaddr = get_virtual_address_from_page(page);
    ASSERT_MSG(vaddr != 0, "get_virtual_address_from_page must succeed");

    uint8_t *ptr = (uint8_t *)vaddr;
    for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
        ptr[i] = (uint8_t)(i ^ 0xA5);
    }
    for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
        ASSERT_MSG(ptr[i] == (uint8_t)(i ^ 0xA5), "page data must round-trip");
    }

    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    TEST_SECTION_END();
}

/// @brief Main test function for zone allocator subsystem.
void test_zone_allocator(void)
{
    test_memory_info_integrity();
    test_memory_virtual_address_validation();
    test_memory_order_calculation();
    test_memory_zone_space_metrics();
    test_memory_zone_total_space_matches();
    test_memory_alloc_free_roundtrip();
    test_memory_alloc_free_order1();
    test_memory_alloc_free_stress();
    test_memory_alloc_free_fragmentation();
    test_memory_lowmem_alloc_free();
    test_memory_lowmem_rejects_highuser();
    test_memory_page_address_roundtrip();
    test_memory_page_write_read();
}
