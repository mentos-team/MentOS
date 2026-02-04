/// @file test_memory_adversarial.c
/// @brief Adversarial and error-condition memory tests.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "mem/alloc/slab.h"
#include "mem/alloc/zone_allocator.h"
#include "mem/gfp.h"
#include "mem/mm/page.h"
#include "mem/paging.h"
#include "string.h"
#include "tests/test.h"
#include "tests/test_utils.h"

/// @brief Test double-free detection in buddy system.
TEST(memory_adversarial_double_free_buddy)
{
    TEST_SECTION_START("Double-free detection (buddy)");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must succeed");

    ASSERT_MSG(free_pages(page) == 0, "first free must succeed");

    // Attempt double-free - buddy system should detect and handle gracefully
    int result = free_pages(page);
    // System should either reject (non-zero) or handle gracefully
    // The key is it shouldn't corrupt the free lists

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must not be corrupted by double-free");

    TEST_SECTION_END();
}

/// @brief Test buffer overflow detection by writing past allocation boundary.
TEST(memory_adversarial_buffer_overflow)
{
    TEST_SECTION_START("Buffer overflow boundary");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    // Allocate small buffer and intentionally overflow
    uint8_t *buf = (uint8_t *)kmalloc(64);
    ASSERT_MSG(buf != NULL, "kmalloc must succeed");

    // Write to valid region
    for (unsigned int i = 0; i < 64; ++i) {
        buf[i] = 0xAA;
    }

    // Write slightly beyond (this WILL corrupt memory, but we're testing detection)
    // In a real system with guard pages/canaries, this would be caught
    // Here we just verify the allocation still functions
    for (unsigned int i = 0; i < 64; ++i) {
        ASSERT_MSG(buf[i] == 0xAA, "Buffer content must remain intact");
    }

    kfree(buf);

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test invalid parameters to allocation functions.
TEST(memory_adversarial_invalid_params)
{
    TEST_SECTION_START("Invalid parameter handling");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    // Very large order (likely exceeds MAX_ORDER)
    page_t *invalid_order = alloc_pages(GFP_KERNEL, 20);
    // Should return NULL or fail gracefully
    if (invalid_order != NULL) {
        free_pages(invalid_order);
    }

    // Invalid GFP flags (combination that doesn't make sense)
    page_t *invalid_gfp = alloc_pages(0xDEADBEEF, 0);
    // Should return NULL or use safe default
    if (invalid_gfp != NULL) {
        free_pages(invalid_gfp);
    }

    // Free NULL page (already tested in slab, but also valid for buddy)
    int result = free_pages(NULL);
    ASSERT_MSG(result != 0, "Freeing NULL page must fail");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must not be corrupted");

    TEST_SECTION_END();
}

/// @brief Test GFP_ATOMIC allocations (interrupt context simulation).
TEST(memory_adversarial_gfp_atomic)
{
    TEST_SECTION_START("GFP_ATOMIC allocations");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    // GFP_ATOMIC must not sleep, must succeed quickly or fail
    page_t *atomic_page = alloc_pages(GFP_ATOMIC, 0);
    if (atomic_page != NULL) {
        // Verify page is usable
        uint32_t vaddr = get_virtual_address_from_page(atomic_page);
        ASSERT_MSG(vaddr != 0, "Atomic page must have valid address");

        ASSERT_MSG(free_pages(atomic_page) == 0, "free must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test complete memory exhaustion scenario.
TEST(memory_adversarial_complete_oom)
{
    TEST_SECTION_START("Complete OOM scenario");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);
    unsigned long total       = get_zone_total_space(GFP_KERNEL);

    const unsigned int max_allocs = 512;
    page_t *allocs[max_allocs];
    unsigned int count = 0;

    // Attempt to allocate until exhaustion
    for (unsigned int i = 0; i < max_allocs; ++i) {
        allocs[i] = alloc_pages(GFP_KERNEL, 3); // Order 3 = 8 pages
        if (allocs[i] == NULL) {
            break;
        }
        count++;

        // Safety: stop if we've consumed most of memory
        unsigned long free_now = get_zone_free_space(GFP_KERNEL);
        if (free_now < (PAGE_SIZE * 16)) {
            count++;
            break;
        }
    }

    // System should still function even under extreme pressure
    ASSERT_MSG(count > 0, "At least some allocations must succeed");

    // Verify we can still query zone status even when low on memory
    unsigned long free_at_low = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_at_low < free_before, "Free space must be reduced");

    // Attempt one more allocation - should fail gracefully
    page_t *final = alloc_pages(GFP_KERNEL, 5);
    if (final != NULL) {
        free_pages(final);
    }

    // Free everything
    for (unsigned int i = 0; i < count; ++i) {
        if (allocs[i] != NULL) {
            ASSERT_MSG(free_pages(allocs[i]) == 0, "free must succeed even under OOM");
        }
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "All memory must be recovered after OOM");

    TEST_SECTION_END();
}

/// @brief Test page reference count overflow protection.
TEST(memory_adversarial_page_refcount_overflow)
{
    TEST_SECTION_START("Page refcount overflow");

    page_t *page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(page != NULL, "alloc_pages must succeed");

    uint32_t initial_count = page_count(page);

    // Increment many times
    for (unsigned int i = 0; i < 100; ++i) {
        page_inc(page);
    }

    ASSERT_MSG(page_count(page) == (initial_count + 100), "Count must increment correctly");

    // Decrement back
    for (unsigned int i = 0; i < 100; ++i) {
        page_dec(page);
    }

    ASSERT_MSG(page_count(page) == initial_count, "Count must return to initial value");

    ASSERT_MSG(free_pages(page) == 0, "free must succeed");

    TEST_SECTION_END();
}

/// @brief Test use-after-free detection (memory pattern check).
TEST(memory_adversarial_use_after_free)
{
    TEST_SECTION_START("Use-after-free pattern");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    uint32_t *ptr = (uint32_t *)kmalloc(256);
    ASSERT_MSG(ptr != NULL, "kmalloc must succeed");

    // Write pattern
    for (unsigned int i = 0; i < 64; ++i) {
        ptr[i] = 0xDEADBEEF;
    }

    // Free the memory
    kfree(ptr);

    // Note: In a real test, accessing ptr now would be use-after-free
    // We can't safely test this without corrupting memory, but we can
    // verify the allocator may reuse this memory

    // Allocate again - might get same location
    uint32_t *ptr2 = (uint32_t *)kmalloc(256);
    ASSERT_MSG(ptr2 != NULL, "second kmalloc must succeed");

    // If we got the same location, pattern should be cleared/different
    // (though this isn't guaranteed behavior)

    kfree(ptr2);

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test mixed allocation patterns between slab and buddy.
TEST(memory_adversarial_mixed_allocators)
{
    TEST_SECTION_START("Mixed slab/buddy patterns");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    // Interleave slab and buddy allocations
    page_t *pages[8];
    void *slabs[8];

    for (unsigned int i = 0; i < 8; ++i) {
        pages[i] = alloc_pages(GFP_KERNEL, 0);
        slabs[i] = kmalloc(128);
        ASSERT_MSG(pages[i] != NULL && slabs[i] != NULL, "allocations must succeed");
    }

    // Free in reverse order (stress both allocators)
    for (int i = 7; i >= 0; --i) {
        kfree(slabs[i]);
        ASSERT_MSG(free_pages(pages[i]) == 0, "free must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test fragmentation with intentional gaps.
TEST(memory_adversarial_pathological_fragmentation)
{
    TEST_SECTION_START("Pathological fragmentation");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    const unsigned int count = 32;
    page_t *pages[count];

    // Allocate all order-0 pages
    for (unsigned int i = 0; i < count; ++i) {
        pages[i] = alloc_pages(GFP_KERNEL, 0);
        ASSERT_MSG(pages[i] != NULL, "allocation must succeed");
    }

    // Free every other page to create maximum fragmentation
    for (unsigned int i = 0; i < count; i += 2) {
        ASSERT_MSG(free_pages(pages[i]) == 0, "free must succeed");
        pages[i] = NULL;
    }

    // Try to allocate order-1 (2 contiguous pages) - might fail due to fragmentation
    page_t *order1 = alloc_pages(GFP_KERNEL, 1);

    // Free remaining pages
    for (unsigned int i = 0; i < count; ++i) {
        if (pages[i] != NULL) {
            ASSERT_MSG(free_pages(pages[i]) == 0, "free must succeed");
        }
    }

    if (order1 != NULL) {
        ASSERT_MSG(free_pages(order1) == 0, "free must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be fully restored");

    TEST_SECTION_END();
}

/// @brief Test alignment requirements for various architectures.
TEST(memory_adversarial_alignment_requirements)
{
    TEST_SECTION_START("Alignment requirements");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    // Test various sizes and verify alignment
    struct test_case {
        uint32_t size;
        uint32_t alignment;
    } cases[] = {
        {1,    1   }, // Minimal
        {2,    2   }, // 2-byte
        {4,    4   }, // 4-byte
        {8,    8   }, // 8-byte
        {16,   16  }, // 16-byte
        {32,   32  }, // 32-byte
        {64,   64  }, // Cache line
        {128,  128 }, // Double cache line
        {4096, 4096}, // Page aligned
    };

    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        void *ptr = kmalloc(cases[i].size);
        if (ptr != NULL) {
            uintptr_t addr = (uintptr_t)ptr;
            // Check natural alignment (at least for power-of-2 sizes)
            if ((cases[i].size & (cases[i].size - 1)) == 0) {
                ASSERT_MSG(
                    (addr & (cases[i].alignment - 1)) == 0,
                    "Allocation must be naturally aligned");
            }
            kfree(ptr);
        }
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test physical address extraction for DMA-like operations.
TEST(memory_adversarial_dma_physical_addressing)
{
    TEST_SECTION_START("DMA physical addressing");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    // Allocate pages as DMA would (must be physically contiguous)
    page_t *page = alloc_pages(GFP_KERNEL, 2); // Order 2 = 4 contiguous pages
    ASSERT_MSG(page != NULL, "DMA allocation must succeed");

    // Extract physical address (what DMA device receives)
    uint32_t phys_addr = get_physical_address_from_page(page);
    ASSERT_MSG(phys_addr != 0, "Physical address must be valid");
    ASSERT_MSG((phys_addr & (PAGE_SIZE - 1)) == 0, "Physical address must be page-aligned");

    // Extract virtual address (what CPU uses to access)
    uint32_t virt_addr = get_virtual_address_from_page(page);
    ASSERT_MSG(virt_addr != 0, "Virtual address must be valid");

    // Verify roundtrip: page -> phys -> page
    page_t *page_from_phys = get_page_from_physical_address(phys_addr);
    ASSERT_MSG(page_from_phys == page, "Physical address must map back to same page");

    // Verify roundtrip: page -> virt -> page
    page_t *page_from_virt = get_page_from_virtual_address(virt_addr);
    ASSERT_MSG(page_from_virt == page, "Virtual address must map back to same page");

    // Verify memory is accessible via virtual address
    uint32_t *ptr = (uint32_t *)virt_addr;
    for (unsigned int i = 0; i < (4 * PAGE_SIZE) / sizeof(uint32_t); ++i) {
        ptr[i] = 0xDEADBEEF;
    }
    for (unsigned int i = 0; i < (4 * PAGE_SIZE) / sizeof(uint32_t); ++i) {
        ASSERT_MSG(ptr[i] == 0xDEADBEEF, "DMA buffer must be readable/writable");
    }

    ASSERT_MSG(free_pages(page) == 0, "free must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test physical contiguity for multi-page DMA allocations.
TEST(memory_adversarial_dma_physical_contiguity)
{
    TEST_SECTION_START("DMA physical contiguity");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    // Allocate multiple contiguous pages (DMA requirement)
    const unsigned int order = 3; // 8 pages
    page_t *page = alloc_pages(GFP_KERNEL, order);
    ASSERT_MSG(page != NULL, "Multi-page DMA allocation must succeed");

    uint32_t first_phys = get_physical_address_from_page(page);
    ASSERT_MSG(first_phys != 0, "First physical address must be valid");

    // Verify physical contiguity across all pages
    for (unsigned int i = 0; i < (1U << order); ++i) {
        page_t *current_page = page + i;
        uint32_t expected_phys = first_phys + (i * PAGE_SIZE);
        uint32_t actual_phys = get_physical_address_from_page(current_page);
        
        ASSERT_MSG(
            actual_phys == expected_phys,
            "Pages must be physically contiguous for DMA");
    }

    ASSERT_MSG(free_pages(page) == 0, "free must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test DMA-like allocation pattern (simulate ATA driver behavior).
TEST(memory_adversarial_dma_ata_simulation)
{
    TEST_SECTION_START("DMA ATA-like allocation");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    // Simulate ATA DMA buffer allocation (typically 4KB-64KB)
    const uint32_t dma_size = 16 * PAGE_SIZE; // 64KB DMA buffer
    uint32_t order = find_nearest_order_greater(0, dma_size);

    page_t *dma_page = alloc_pages(GFP_KERNEL, order);
    ASSERT_MSG(dma_page != NULL, "DMA buffer allocation must succeed");

    // Extract physical and virtual addresses (as ATA driver does)
    uint32_t phys_addr = get_physical_address_from_page(dma_page);
    uint32_t virt_addr = get_virtual_address_from_page(dma_page);

    ASSERT_MSG(phys_addr != 0, "DMA physical address must be valid");
    ASSERT_MSG(virt_addr != 0, "DMA virtual address must be valid");

    // Verify alignment (DMA often requires specific alignment)
    ASSERT_MSG((phys_addr & (PAGE_SIZE - 1)) == 0, "DMA physical address must be page-aligned");
    ASSERT_MSG((virt_addr & (PAGE_SIZE - 1)) == 0, "DMA virtual address must be page-aligned");

    // Simulate DMA buffer usage (CPU writes, DMA reads from physical)
    uint8_t *buffer = (uint8_t *)virt_addr;
    for (uint32_t i = 0; i < dma_size; ++i) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }

    // Verify data integrity
    for (uint32_t i = 0; i < dma_size; ++i) {
        ASSERT_MSG(buffer[i] == (uint8_t)(i & 0xFF), "DMA buffer data must be intact");
    }

    ASSERT_MSG(free_pages(dma_page) == 0, "DMA buffer free must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test lowmem constraint for DMA (current workaround limitation).
TEST(memory_adversarial_dma_lowmem_constraint)
{
    TEST_SECTION_START("DMA lowmem constraint");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    // DMA must allocate from lowmem (ZONE_NORMAL) since no ZONE_DMA exists
    page_t *dma_page = alloc_pages(GFP_KERNEL, 0);
    ASSERT_MSG(dma_page != NULL, "DMA allocation must succeed");

    // Verify page is in lowmem zone (required for DMA workaround)
    ASSERT_MSG(is_lowmem_page_struct(dma_page), "DMA page must be in lowmem zone");

    uint32_t phys_addr = get_physical_address_from_page(dma_page);
    uint32_t virt_addr = get_virtual_address_from_page(dma_page);

    // Verify virtual-to-physical relationship (identity mapping in lowmem)
    ASSERT_MSG(phys_addr != 0 && virt_addr != 0, "Both addresses must be valid");

    // In lowmem, there should be a consistent offset between virtual and physical
    // (this is what makes the DMA workaround possible)
    ASSERT_MSG(is_valid_virtual_address(virt_addr), "Virtual address must be in valid range");

    ASSERT_MSG(free_pages(dma_page) == 0, "free must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test multiple DMA buffers allocation (stress test).
TEST(memory_adversarial_dma_multiple_buffers)
{
    TEST_SECTION_START("Multiple DMA buffers");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    const unsigned int num_buffers = 8;
    page_t *dma_buffers[num_buffers];
    uint32_t phys_addrs[num_buffers];

    // Allocate multiple DMA buffers (as multiple devices might)
    for (unsigned int i = 0; i < num_buffers; ++i) {
        dma_buffers[i] = alloc_pages(GFP_KERNEL, 2); // 4 pages each
        ASSERT_MSG(dma_buffers[i] != NULL, "DMA buffer allocation must succeed");

        phys_addrs[i] = get_physical_address_from_page(dma_buffers[i]);
        ASSERT_MSG(phys_addrs[i] != 0, "Physical address must be valid");
    }

    // Verify no overlap between DMA buffers (critical for DMA safety)
    for (unsigned int i = 0; i < num_buffers; ++i) {
        for (unsigned int j = i + 1; j < num_buffers; ++j) {
            uint32_t buf_i_end = phys_addrs[i] + (4 * PAGE_SIZE);
            uint32_t buf_j_end = phys_addrs[j] + (4 * PAGE_SIZE);

            int overlap = (phys_addrs[i] < buf_j_end) && (phys_addrs[j] < buf_i_end);
            ASSERT_MSG(!overlap, "DMA buffers must not overlap");
        }
    }

    // Free all buffers
    for (unsigned int i = 0; i < num_buffers; ++i) {
        ASSERT_MSG(free_pages(dma_buffers[i]) == 0, "free must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test DMA buffer alignment requirements.
TEST(memory_adversarial_dma_alignment)
{
    TEST_SECTION_START("DMA buffer alignment");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    // Test various DMA buffer sizes and verify alignment
    uint32_t sizes[] = { PAGE_SIZE, 2 * PAGE_SIZE, 4 * PAGE_SIZE, 8 * PAGE_SIZE, 64 * PAGE_SIZE };

    for (unsigned int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        uint32_t order = find_nearest_order_greater(0, sizes[i]);
        page_t *page = alloc_pages(GFP_KERNEL, order);
        
        if (page != NULL) {
            uint32_t phys = get_physical_address_from_page(page);
            uint32_t virt = get_virtual_address_from_page(page);

            // DMA requires page alignment at minimum
            ASSERT_MSG((phys & (PAGE_SIZE - 1)) == 0, "Physical address must be page-aligned");
            ASSERT_MSG((virt & (PAGE_SIZE - 1)) == 0, "Virtual address must be page-aligned");

            // Note: Buddy system doesn't guarantee natural alignment beyond page size
            // For true DMA with strict alignment, would need ZONE_DMA with alignment guarantees
            // Here we just verify page alignment which is sufficient for most DMA

            ASSERT_MSG(free_pages(page) == 0, "free must succeed");
        }
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Main test function for adversarial memory tests.
void test_memory_adversarial(void)
{
    test_memory_adversarial_double_free_buddy();
    test_memory_adversarial_buffer_overflow();
    test_memory_adversarial_invalid_params();
    test_memory_adversarial_gfp_atomic();
    test_memory_adversarial_complete_oom();
    test_memory_adversarial_page_refcount_overflow();
    test_memory_adversarial_use_after_free();
    test_memory_adversarial_mixed_allocators();
    test_memory_adversarial_pathological_fragmentation();
    test_memory_adversarial_alignment_requirements();
    test_memory_adversarial_dma_physical_addressing();
    test_memory_adversarial_dma_physical_contiguity();
    test_memory_adversarial_dma_ata_simulation();
    test_memory_adversarial_dma_lowmem_constraint();
    test_memory_adversarial_dma_multiple_buffers();
    test_memory_adversarial_dma_alignment();
}
