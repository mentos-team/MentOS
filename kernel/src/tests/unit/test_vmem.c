/// @file test_vmem.c
/// @brief VMEM mapping tests.
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
#include "mem/mm/vmem.h"
#include "mem/paging.h"
#include "tests/test.h"
#include "tests/test_utils.h"

/// @brief Test vmem virtual allocation and unmap.
TEST(memory_vmem_alloc_unmap)
{
    TEST_SECTION_START("VMEM alloc/unmap");

    virt_map_page_t *vpage = vmem_map_alloc_virtual(PAGE_SIZE);
    ASSERT_MSG(vpage != NULL, "vmem_map_alloc_virtual must succeed");
    ASSERT_MSG(vmem_unmap_virtual_address_page(vpage) == 0, "vmem_unmap_virtual_address_page must succeed");

    TEST_SECTION_END();
}

/// @brief Test multi-page virtual allocation and unmap.
TEST(memory_vmem_alloc_unmap_multi)
{
    TEST_SECTION_START("VMEM alloc/unmap multi-page");

    virt_map_page_t *vpage = vmem_map_alloc_virtual(PAGE_SIZE * 3);
    ASSERT_MSG(vpage != NULL, "vmem_map_alloc_virtual must succeed");
    ASSERT_MSG(vmem_unmap_virtual_address_page(vpage) == 0, "vmem_unmap_virtual_address_page must succeed");

    TEST_SECTION_END();
}

/// @brief Test mapping physical pages into virtual memory and unmapping.
TEST(memory_vmem_map_physical)
{
    TEST_SECTION_START("VMEM map physical pages");

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must return a valid page");

    uint32_t vaddr = vmem_map_physical_pages(page, 1);
    ASSERT_MSG(vaddr != 0, "vmem_map_physical_pages must return a valid address");
    ASSERT_MSG(is_valid_virtual_address(vaddr) == 1, "mapped virtual address must be valid");
    ASSERT_MSG(vmem_unmap_virtual_address(vaddr) == 0, "vmem_unmap_virtual_address must succeed");

    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    TEST_SECTION_END();
}

/// @brief Test write/read via vmem mapping and lowmem mapping.
TEST(memory_vmem_write_read)
{
    TEST_SECTION_START("VMEM write/read");

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must return a valid page");

    uint32_t vaddr = vmem_map_physical_pages(page, 1);
    ASSERT_MSG(vaddr != 0, "vmem_map_physical_pages must return a valid address");

    uint8_t *mapped = (uint8_t *)vaddr;
    for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
        mapped[i] = (uint8_t)(0x3C ^ i);
    }

    uint32_t lowmem = get_virtual_address_from_page(page);
    ASSERT_MSG(lowmem != 0, "get_virtual_address_from_page must succeed");
    uint8_t *lowptr = (uint8_t *)lowmem;
    for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
        ASSERT_MSG(lowptr[i] == (uint8_t)(0x3C ^ i), "vmem mapping must hit same physical page");
    }

    ASSERT_MSG(vmem_unmap_virtual_address(vaddr) == 0, "vmem_unmap_virtual_address must succeed");
    ASSERT_MSG(free_pages(page) == 0, "free_pages must succeed");

    TEST_SECTION_END();
}

/// @brief Test detection of invalid virtual addresses for vmem.
TEST(memory_vmem_invalid_address_detected)
{
    TEST_SECTION_START("VMEM invalid address detected");

    uint32_t invalid_addr    = memory.low_mem.virt_end;
    unsigned long total_high = get_zone_total_space(GFP_HIGHUSER);
    if (total_high > 0) {
        invalid_addr = memory.high_mem.virt_end;
    }

    ASSERT_MSG(is_valid_virtual_address(invalid_addr) == 0, "invalid address must be rejected");

    TEST_SECTION_END();
}

/// @brief Stress vmem alloc/unmap to detect leaks.
TEST(memory_vmem_stress)
{
    TEST_SECTION_START("VMEM stress");

    const unsigned int rounds = 16;
    for (unsigned int i = 0; i < rounds; ++i) {
        virt_map_page_t *vpage = vmem_map_alloc_virtual(PAGE_SIZE * 2);
        ASSERT_MSG(vpage != NULL, "vmem_map_alloc_virtual must succeed");
        ASSERT_MSG(vmem_unmap_virtual_address_page(vpage) == 0, "vmem_unmap_virtual_address_page must succeed");
    }

    TEST_SECTION_END();
}

/// @brief Main test function for vmem subsystem.
void test_vmem(void)
{
    test_memory_vmem_alloc_unmap();
    test_memory_vmem_alloc_unmap_multi();
    test_memory_vmem_map_physical();
    test_memory_vmem_write_read();
    test_memory_vmem_invalid_address_detected();
    test_memory_vmem_stress();
}
