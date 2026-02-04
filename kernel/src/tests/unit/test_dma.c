/// @file test_dma.c
/// @brief DMA zone and allocation tests.
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

/// @brief Validate DMA zone metadata and virtual mapping.
TEST(dma_zone_integrity)
{
    TEST_SECTION_START("DMA zone integrity");

    ASSERT_MSG(memory.dma_mem.size > 0, "DMA zone size must be > 0");
    ASSERT_MSG(memory.dma_mem.start_addr < memory.dma_mem.end_addr, "DMA zone physical range invalid");
    ASSERT_MSG(
        memory.dma_mem.size == (memory.dma_mem.end_addr - memory.dma_mem.start_addr),
        "DMA zone size must match physical range");
    ASSERT_MSG(memory.dma_mem.end_addr <= 0x01000000, "DMA zone must fit within 16MB ISA limit");
    ASSERT_MSG((memory.dma_mem.start_addr & (PAGE_SIZE - 1)) == 0, "DMA zone start must be page-aligned");
    ASSERT_MSG((memory.dma_mem.end_addr & (PAGE_SIZE - 1)) == 0, "DMA zone end must be page-aligned");

    ASSERT_MSG(memory.dma_mem.virt_start < memory.dma_mem.virt_end, "DMA zone virtual range invalid");
    ASSERT_MSG(
        memory.dma_mem.virt_end == (memory.dma_mem.virt_start + memory.dma_mem.size),
        "DMA zone virtual range must match size");
    ASSERT_MSG((memory.dma_mem.virt_start & (PAGE_SIZE - 1)) == 0, "DMA zone virt start must be page-aligned");
    ASSERT_MSG((memory.dma_mem.virt_end & (PAGE_SIZE - 1)) == 0, "DMA zone virt end must be page-aligned");

    ASSERT_MSG(is_valid_virtual_address(memory.dma_mem.virt_start) == 1, "DMA virt start must be valid");
    ASSERT_MSG(is_valid_virtual_address(memory.dma_mem.virt_end - 1) == 1, "DMA virt end-1 must be valid");

    TEST_SECTION_END();
}

/// @brief Test small order allocations and address translations in DMA zone.
TEST(dma_order_allocations_and_translation)
{
    TEST_SECTION_START("DMA order allocations and translation");

    unsigned long free_before = get_zone_free_space(GFP_DMA);

    for (uint32_t order = 0; order <= 5; ++order) {
        page_t *page = alloc_pages(GFP_DMA, order);
        ASSERT_MSG(page != NULL, "DMA allocation must succeed");
        ASSERT_MSG(is_dma_page_struct(page), "DMA allocation must come from DMA zone");

        uint32_t phys = get_physical_address_from_page(page);
        uint32_t virt = get_virtual_address_from_page(page);

        ASSERT_MSG(phys >= memory.dma_mem.start_addr && phys < memory.dma_mem.end_addr, "DMA physical address must be inside DMA zone");
        ASSERT_MSG(virt >= memory.dma_mem.virt_start && virt < memory.dma_mem.virt_end, "DMA virtual address must be inside DMA zone");
        ASSERT_MSG((phys & (PAGE_SIZE - 1)) == 0, "DMA physical address must be page-aligned");
        ASSERT_MSG((virt & (PAGE_SIZE - 1)) == 0, "DMA virtual address must be page-aligned");

        page_t *from_phys = get_page_from_physical_address(phys);
        page_t *from_virt = get_page_from_virtual_address(virt);
        ASSERT_MSG(from_phys == page, "Physical address must map back to same page");
        ASSERT_MSG(from_virt == page, "Virtual address must map back to same page");

        ASSERT_MSG(free_pages(page) == 0, "DMA free must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_DMA);
    ASSERT_MSG(free_after >= free_before, "DMA free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test physical contiguity for DMA multi-page allocations.
TEST(dma_physical_contiguity)
{
    TEST_SECTION_START("DMA physical contiguity");

    unsigned long free_before = get_zone_free_space(GFP_DMA);

    const unsigned int order = 4; // 16 pages
    page_t *page             = alloc_pages(GFP_DMA, order);
    ASSERT_MSG(page != NULL, "DMA allocation must succeed");

    uint32_t first_phys = get_physical_address_from_page(page);
    ASSERT_MSG(first_phys >= memory.dma_mem.start_addr && first_phys < memory.dma_mem.end_addr, "First physical address must be inside DMA zone");

    for (unsigned int i = 0; i < (1U << order); ++i) {
        page_t *current_page = page + i;
        uint32_t expected    = first_phys + (i * PAGE_SIZE);
        uint32_t actual      = get_physical_address_from_page(current_page);
        ASSERT_MSG(actual == expected, "DMA pages must be physically contiguous");
    }

    ASSERT_MSG(free_pages(page) == 0, "DMA free must succeed");

    unsigned long free_after = get_zone_free_space(GFP_DMA);
    ASSERT_MSG(free_after >= free_before, "DMA free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test DMA buffer access and data integrity for ATA-like sizes.
TEST(dma_ata_like_buffer)
{
    TEST_SECTION_START("DMA ATA-like buffer");

    unsigned long free_before = get_zone_free_space(GFP_DMA);

    const uint32_t dma_size = 16 * PAGE_SIZE; // 64KB
    uint32_t order          = find_nearest_order_greater(0, dma_size);

    page_t *dma_page = alloc_pages(GFP_DMA, order);
    ASSERT_MSG(dma_page != NULL, "DMA buffer allocation must succeed");

    uint32_t phys_addr = get_physical_address_from_page(dma_page);
    uint32_t virt_addr = get_virtual_address_from_page(dma_page);

    ASSERT_MSG(phys_addr >= memory.dma_mem.start_addr && phys_addr < memory.dma_mem.end_addr, "DMA physical address must be inside DMA zone");
    ASSERT_MSG(virt_addr >= memory.dma_mem.virt_start && virt_addr < memory.dma_mem.virt_end, "DMA virtual address must be inside DMA zone");
    ASSERT_MSG((phys_addr & (PAGE_SIZE - 1)) == 0, "DMA physical address must be page-aligned");
    ASSERT_MSG((virt_addr & (PAGE_SIZE - 1)) == 0, "DMA virtual address must be page-aligned");

    uint8_t *buffer = (uint8_t *)virt_addr;
    for (uint32_t i = 0; i < dma_size; ++i) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }
    for (uint32_t i = 0; i < dma_size; ++i) {
        ASSERT_MSG(buffer[i] == (uint8_t)(i & 0xFF), "DMA buffer data must be intact");
    }

    ASSERT_MSG(free_pages(dma_page) == 0, "DMA buffer free must succeed");

    unsigned long free_after = get_zone_free_space(GFP_DMA);
    ASSERT_MSG(free_after >= free_before, "DMA free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test multiple DMA buffers and ensure no overlap.
TEST(dma_multiple_buffers_no_overlap)
{
    TEST_SECTION_START("DMA multiple buffers");

    unsigned long free_before = get_zone_free_space(GFP_DMA);

    const unsigned int num_buffers = 8;
    page_t *dma_buffers[num_buffers];
    uint32_t phys_addrs[num_buffers];

    for (unsigned int i = 0; i < num_buffers; ++i) {
        dma_buffers[i] = alloc_pages(GFP_DMA, 2); // 4 pages each
        ASSERT_MSG(dma_buffers[i] != NULL, "DMA buffer allocation must succeed");

        phys_addrs[i] = get_physical_address_from_page(dma_buffers[i]);
        ASSERT_MSG(phys_addrs[i] >= memory.dma_mem.start_addr && phys_addrs[i] < memory.dma_mem.end_addr, "DMA physical address must be inside DMA zone");
    }

    for (unsigned int i = 0; i < num_buffers; ++i) {
        for (unsigned int j = i + 1; j < num_buffers; ++j) {
            uint32_t buf_i_end = phys_addrs[i] + (4 * PAGE_SIZE);
            uint32_t buf_j_end = phys_addrs[j] + (4 * PAGE_SIZE);
            int overlap        = (phys_addrs[i] < buf_j_end) && (phys_addrs[j] < buf_i_end);
            ASSERT_MSG(!overlap, "DMA buffers must not overlap");
        }
    }

    for (unsigned int i = 0; i < num_buffers; ++i) {
        ASSERT_MSG(free_pages(dma_buffers[i]) == 0, "DMA free must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_DMA);
    ASSERT_MSG(free_after >= free_before, "DMA free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test DMA alignment for various buffer sizes.
TEST(dma_alignment)
{
    TEST_SECTION_START("DMA alignment");

    unsigned long free_before = get_zone_free_space(GFP_DMA);

    uint32_t sizes[] = {PAGE_SIZE, 2 * PAGE_SIZE, 4 * PAGE_SIZE, 8 * PAGE_SIZE, 64 * PAGE_SIZE};

    for (unsigned int i = 0; i < (sizeof(sizes) / sizeof(sizes[0])); ++i) {
        uint32_t order = find_nearest_order_greater(0, sizes[i]);
        page_t *page   = alloc_pages(GFP_DMA, order);
        ASSERT_MSG(page != NULL, "DMA allocation must succeed");

        uint32_t phys = get_physical_address_from_page(page);
        uint32_t virt = get_virtual_address_from_page(page);

        ASSERT_MSG((phys & (PAGE_SIZE - 1)) == 0, "Physical address must be page-aligned");
        ASSERT_MSG((virt & (PAGE_SIZE - 1)) == 0, "Virtual address must be page-aligned");

        ASSERT_MSG(free_pages(page) == 0, "DMA free must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_DMA);
    ASSERT_MSG(free_after >= free_before, "DMA free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test partial exhaustion and recovery of the DMA zone.
TEST(dma_partial_exhaustion_recovery)
{
    TEST_SECTION_START("DMA partial exhaustion and recovery");

    unsigned long free_before = get_zone_free_space(GFP_DMA);

    const uint32_t block_order     = 8; // 1MB
    const unsigned long block_size = (1UL << block_order) * PAGE_SIZE;
    unsigned long max_blocks       = (block_size == 0) ? 0 : (memory.dma_mem.size / block_size);
    unsigned long target_blocks    = (max_blocks >= 4) ? 4 : ((max_blocks >= 2) ? 2 : 1);

    page_t *blocks[4] = {NULL};
    for (unsigned long i = 0; i < target_blocks; ++i) {
        blocks[i] = alloc_pages(GFP_DMA, block_order);
        ASSERT_MSG(blocks[i] != NULL, "DMA block allocation must succeed");
    }

    unsigned long free_mid = get_zone_free_space(GFP_DMA);
    ASSERT_MSG(free_mid < free_before, "DMA free space must decrease after allocations");

    for (unsigned long i = 0; i < target_blocks; ++i) {
        ASSERT_MSG(free_pages(blocks[i]) == 0, "DMA block free must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_DMA);
    ASSERT_MSG(free_after >= free_before, "DMA free space must be restored");

    TEST_SECTION_END();
}

/// @brief Main test function for DMA tests.
void test_dma(void)
{
    test_dma_zone_integrity();
    test_dma_order_allocations_and_translation();
    test_dma_physical_contiguity();
    test_dma_ata_like_buffer();
    test_dma_multiple_buffers_no_overlap();
    test_dma_alignment();
    test_dma_partial_exhaustion_recovery();
}
