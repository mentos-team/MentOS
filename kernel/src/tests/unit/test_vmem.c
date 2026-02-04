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

/// @brief Main test function for vmem subsystem.
void test_vmem(void)
{
    test_memory_vmem_alloc_unmap();
    test_memory_vmem_map_physical();
}
