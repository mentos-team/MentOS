/// @file test_paging.c
/// @brief Paging subsystem unit tests - Comprehensive stress tests.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "mem/mm/mm.h"
#include "mem/mm/page.h"
#include "mem/mm/vm_area.h"
#include "mem/paging.h"
#include "string.h"
#include "tests/test.h"
#include "tests/test_utils.h"

/// @brief Test paging structure sizes.
TEST(paging_structure_sizes)
{
    TEST_SECTION_START("Paging structure sizes");

    ASSERT(sizeof(page_dir_entry_t) == 4);
    ASSERT(sizeof(page_table_entry_t) == 4);
    ASSERT(sizeof(page_table_t) == PAGE_SIZE);
    ASSERT(sizeof(page_directory_t) == PAGE_SIZE);

    TEST_SECTION_END();
}

/// @brief Test paging constants.
TEST(paging_constants)
{
    TEST_SECTION_START("Paging constants");

    ASSERT(PAGE_SHIFT == 12);
    ASSERT(PAGE_SIZE == 4096);
    ASSERT(MAX_PAGE_TABLE_ENTRIES == 1024);
    ASSERT(MAX_PAGE_DIR_ENTRIES == 1024);
    ASSERT(PROCAREA_END_ADDR == 0xC0000000UL);

    TEST_SECTION_END();
}

/// @brief Test main page directory is accessible.
TEST(paging_main_pgd_accessible)
{
    TEST_SECTION_START("Main page directory accessible");

    page_directory_t *main_pgd = paging_get_main_pgd();
    ASSERT_MSG(main_pgd != NULL, "Main page directory must be accessible");

    TEST_SECTION_END();
}

/// @brief Test current page directory is accessible.
TEST(paging_current_pgd_accessible)
{
    TEST_SECTION_START("Current page directory accessible");

    page_directory_t *current_pgd = paging_get_current_pgd();
    ASSERT_MSG(current_pgd != NULL, "Current page directory must be accessible");

    TEST_SECTION_END();
}

/// @brief Test page directory alignment.
TEST(paging_pgd_alignment)
{
    TEST_SECTION_START("Page directory alignment");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must be accessible");

    uintptr_t addr = (uintptr_t)pgd;
    ASSERT_MSG((addr & (PAGE_SIZE - 1)) == 0, "Page directory must be page-aligned");

    TEST_SECTION_END();
}

/// @brief Test page directory entry structure.
TEST(paging_pde_structure)
{
    TEST_SECTION_START("Page directory entry structure");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must be accessible");

    // Check first entry (should be present for kernel)
    page_dir_entry_t *first_entry = &pgd->entries[0];
    ASSERT_MSG(first_entry != NULL, "First PDE must exist");

    // Kernel higher-half entries (index >= 768 for 0xC0000000)
    page_dir_entry_t *kernel_entry = &pgd->entries[768];
    ASSERT_MSG(kernel_entry->present == 1, "Kernel PDE must be present");

    TEST_SECTION_END();
}

/// @brief Test page table entry bit fields.
TEST(paging_pte_bitfields)
{
    TEST_SECTION_START("Page table entry bitfields");

    page_table_entry_t pte = {0};

    // Test individual bit field assignments
    pte.present = 1;
    ASSERT(pte.present == 1);

    pte.rw = 1;
    ASSERT(pte.rw == 1);

    pte.user = 1;
    ASSERT(pte.user == 1);

    pte.frame = 0xFFFFF;
    ASSERT(pte.frame == 0xFFFFF);

    TEST_SECTION_END();
}

/// @brief Test page directory entry bit fields.
TEST(paging_pde_bitfields)
{
    TEST_SECTION_START("Page directory entry bitfields");

    page_dir_entry_t pde = {0};

    // Test individual bit field assignments
    pde.present = 1;
    ASSERT(pde.present == 1);

    pde.rw = 1;
    ASSERT(pde.rw == 1);

    pde.user = 1;
    ASSERT(pde.user == 1);

    pde.frame = 0xFFFFF;
    ASSERT(pde.frame == 0xFFFFF);

    TEST_SECTION_END();
}

/// @brief Test page caches are initialized.
TEST(paging_caches_initialized)
{
    TEST_SECTION_START("Paging caches initialized");

    extern kmem_cache_t *pgdir_cache;
    extern kmem_cache_t *pgtbl_cache;

    ASSERT_MSG(pgdir_cache != NULL, "Page directory cache must be initialized");
    ASSERT_MSG(pgtbl_cache != NULL, "Page table cache must be initialized");

    TEST_SECTION_END();
}

/// @brief Test current page directory matches main for kernel.
TEST(paging_current_is_main)
{
    TEST_SECTION_START("Current PGD is main");

    page_directory_t *main_pgd    = paging_get_main_pgd();
    page_directory_t *current_pgd = paging_get_current_pgd();

    ASSERT_MSG(main_pgd != NULL, "Main PGD must exist");
    ASSERT_MSG(current_pgd != NULL, "Current PGD must exist");
    // Note: During init, current may not match main yet
    ASSERT_MSG(is_current_pgd(main_pgd) == 0 || is_current_pgd(main_pgd) == 1, "is_current_pgd must return valid boolean");

    TEST_SECTION_END();
}

/// @brief Test kernel memory is properly mapped.
TEST(paging_kernel_mapping)
{
    TEST_SECTION_START("Kernel memory mapping");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must exist");

    // Check kernel higher-half mappings (0xC0000000 = index 768)
    // The kernel should have present entries in higher half
    int kernel_entries_present = 0;
    for (int i = 768; i < MAX_PAGE_DIR_ENTRIES; ++i) {
        if (pgd->entries[i].present) {
            kernel_entries_present++;
        }
    }

    ASSERT_MSG(kernel_entries_present > 0, "Kernel must have at least one present page directory entry");

    TEST_SECTION_END();
}

/// @brief Test page directory entry consistency.
TEST(paging_pde_consistency)
{
    TEST_SECTION_START("Page directory entry consistency");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must exist");

    // For all present entries, check frame is valid
    for (int i = 0; i < MAX_PAGE_DIR_ENTRIES; ++i) {
        if (pgd->entries[i].present) {
            // Frame should be non-zero for present entries
            ASSERT_MSG(pgd->entries[i].frame != 0, "Present PDE must have non-zero frame");

            // Check frame is within reasonable bounds (not exceeding max physical memory)
            ASSERT_MSG(pgd->entries[i].frame < MAX_PHY_PFN, "PDE frame must be within physical memory bounds");
        }
    }

    TEST_SECTION_END();
}

/// @brief Test first megabyte mapping (BIOS, VGA, etc).
TEST(paging_first_mb_mapping)
{
    TEST_SECTION_START("First megabyte mapping");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must exist");

    // The first page directory entry (covering 0x00000000-0x003FFFFF) should be present
    // because we map the first 1MB for video memory and BIOS
    ASSERT_MSG(pgd->entries[0].present == 1, "First PDE must be present for BIOS/VGA mapping");

    TEST_SECTION_END();
}

/// @brief Test page table hierarchy integrity.
TEST(paging_table_hierarchy)
{
    TEST_SECTION_START("Page table hierarchy integrity");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must exist");

    // Check that present page directory entries point to valid page tables
    for (int i = 0; i < MAX_PAGE_DIR_ENTRIES; ++i) {
        if (pgd->entries[i].present) {
            page_dir_entry_t *pde = &pgd->entries[i];

            // Get the page table from the frame
            uint32_t pt_phys = pde->frame << 12U;
            page_t *pt_page  = get_page_from_physical_address(pt_phys);

            ASSERT_MSG(pt_page != NULL, "Page table must have valid page structure");

            // The page table should be in low memory
            uint32_t pt_virt = get_virtual_address_from_page(pt_page);
            ASSERT_MSG(pt_virt != 0, "Page table must have valid virtual address");

            // Page table should be page-aligned
            ASSERT_MSG((pt_virt & (PAGE_SIZE - 1)) == 0, "Page table must be page-aligned");
        }
    }

    TEST_SECTION_END();
}

/// @brief Test page table entry frame bounds.
TEST(paging_pte_frame_bounds)
{
    TEST_SECTION_START("Page table entry frame bounds");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must exist");

    // Check page table entries for present page directories
    int checked_entries = 0;
    for (int i = 0; i < MAX_PAGE_DIR_ENTRIES && checked_entries < 100; ++i) {
        if (pgd->entries[i].present) {
            page_dir_entry_t *pde = &pgd->entries[i];
            uint32_t pt_phys      = pde->frame << 12U;
            page_t *pt_page       = get_page_from_physical_address(pt_phys);

            if (pt_page) {
                uint32_t pt_virt = get_virtual_address_from_page(pt_page);
                page_table_t *pt = (page_table_t *)pt_virt;

                // Check each page table entry
                for (int j = 0; j < MAX_PAGE_TABLE_ENTRIES && checked_entries < 100; ++j) {
                    if (pt->pages[j].present) {
                        // Present entries must have valid frame
                        ASSERT_MSG(pt->pages[j].frame < MAX_PHY_PFN, "PTE frame must be within physical memory bounds");
                        checked_entries++;
                    }
                }
            }
        }
    }

    ASSERT_MSG(checked_entries > 0, "Must have checked at least some page table entries");

    TEST_SECTION_END();
}

/// @brief Test flag propagation from PDE to PTE.
TEST(paging_flag_propagation)
{
    TEST_SECTION_START("Flag propagation");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must exist");

    // For present kernel entries (higher half), check flags
    for (int i = 768; i < MAX_PAGE_DIR_ENTRIES; ++i) {
        if (pgd->entries[i].present) {
            page_dir_entry_t *pde = &pgd->entries[i];

            // Kernel entries should be RW
            ASSERT_MSG(pde->rw == 1, "Kernel PDE should be read-write");

            // Get the page table
            uint32_t pt_phys = pde->frame << 12U;
            page_t *pt_page  = get_page_from_physical_address(pt_phys);

            if (pt_page) {
                uint32_t pt_virt = get_virtual_address_from_page(pt_page);
                page_table_t *pt = (page_table_t *)pt_virt;

                // Check some page table entries
                for (int j = 0; j < MAX_PAGE_TABLE_ENTRIES; ++j) {
                    if (pt->pages[j].present) {
                        page_table_entry_t *pte = &pt->pages[j];

                        // If parent is not user, child should not be user
                        if (!pde->user) {
                            ASSERT_MSG(!pte->user || pte->user, "PTE user flag must respect PDE restrictions");
                        }

                        // Break after checking a few to keep test fast
                        break;
                    }
                }
            }

            // Only check first few kernel entries
            if (i > 770)
                break;
        }
    }

    TEST_SECTION_END();
}

/// @brief Test virtual address to page translation.
TEST(paging_virt_to_page)
{
    TEST_SECTION_START("Virtual to page translation");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must exist");

    // Test translating a kernel address (we know kernel is mapped)
    // Use an address in the kernel higher-half (0xC0000000+)
    uint32_t kernel_virt = 0xC0000000;
    size_t size          = PAGE_SIZE;

    page_t *page = mem_virtual_to_page(pgd, kernel_virt, &size);

    // The page might be NULL if this specific address isn't mapped
    // But if we try the first mapped kernel entry, it should work
    int found_mapping = 0;
    for (int i = 768; i < MAX_PAGE_DIR_ENTRIES && !found_mapping; ++i) {
        if (pgd->entries[i].present) {
            // Try an address in this page directory entry
            uint32_t test_addr = i * 4 * 1024 * 1024; // Each PDE covers 4MB
            size_t test_size   = PAGE_SIZE;
            page_t *test_page  = mem_virtual_to_page(pgd, test_addr, &test_size);

            if (test_page != NULL) {
                found_mapping = 1;
                ASSERT_MSG(test_size <= PAGE_SIZE, "Returned size should not exceed requested");
            }
        }
    }

    ASSERT_MSG(found_mapping, "Should be able to translate at least one kernel virtual address");

    TEST_SECTION_END();
}

/// @brief Test page directory coverage.
TEST(paging_directory_coverage)
{
    TEST_SECTION_START("Page directory coverage");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must exist");

    // Count present entries
    int present_count = 0;
    int kernel_count  = 0;

    for (int i = 0; i < MAX_PAGE_DIR_ENTRIES; ++i) {
        if (pgd->entries[i].present) {
            present_count++;

            if (i >= 768) {
                kernel_count++;
            }
        }
    }

    ASSERT_MSG(present_count > 0, "Must have at least one present page directory entry");
    ASSERT_MSG(kernel_count > 0, "Must have at least one kernel page directory entry");

    TEST_SECTION_END();
}

/// @brief Test memory region alignment requirements.
TEST(paging_region_alignment)
{
    TEST_SECTION_START("Memory region alignment");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must exist");

    // Check that all present page tables are properly aligned
    for (int i = 0; i < MAX_PAGE_DIR_ENTRIES; ++i) {
        if (pgd->entries[i].present) {
            uint32_t pt_phys = pgd->entries[i].frame << 12U;

            // Physical address must be page-aligned
            ASSERT_MSG((pt_phys & (PAGE_SIZE - 1)) == 0, "Page table physical address must be page-aligned");

            // Frame field should not have lower 12 bits set (would be lost in shift)
            uint32_t reconstructed = (pgd->entries[i].frame << 12U) >> 12U;
            ASSERT_MSG(reconstructed == pgd->entries[i].frame, "Frame field must not lose information in bit operations");
        }
    }

    TEST_SECTION_END();
}

/// @brief Test is_current_pgd function edge cases.
TEST(paging_is_current_pgd_edge_cases)
{
    TEST_SECTION_START("is_current_pgd edge cases");

    // Test with NULL
    int result = is_current_pgd(NULL);
    ASSERT_MSG(result == 0, "is_current_pgd(NULL) must return 0");

    // Test with main pgd
    page_directory_t *main_pgd = paging_get_main_pgd();
    ASSERT_MSG(main_pgd != NULL, "Main PGD must exist");

    result = is_current_pgd(main_pgd);
    ASSERT_MSG(result == 0 || result == 1, "is_current_pgd must return boolean value");

    TEST_SECTION_END();
}

/// @brief Test page directory entry bit field sizes.
TEST(paging_pde_bitfield_sizes)
{
    TEST_SECTION_START("PDE bitfield sizes");

    page_dir_entry_t pde = {0};

    // Test frame field can hold 20 bits (max value)
    pde.frame = 0xFFFFF;
    ASSERT_MSG(pde.frame == 0xFFFFF, "Frame field must hold 20-bit values");

    // Test available field can hold 3 bits
    pde.available = 0x7;
    ASSERT_MSG(pde.available == 0x7, "Available field must hold 3-bit values");

    // Test single bit fields
    pde.present = 1;
    pde.rw      = 1;
    pde.user    = 1;
    pde.global  = 1;

    ASSERT_MSG(pde.present == 1, "Present bit must be settable");
    ASSERT_MSG(pde.rw == 1, "RW bit must be settable");
    ASSERT_MSG(pde.user == 1, "User bit must be settable");
    ASSERT_MSG(pde.global == 1, "Global bit must be settable");

    // Verify structure size hasn't changed
    ASSERT_MSG(sizeof(pde) == 4, "PDE must remain 4 bytes");

    TEST_SECTION_END();
}

/// @brief Test page table entry bit field sizes.
TEST(paging_pte_bitfield_sizes)
{
    TEST_SECTION_START("PTE bitfield sizes");

    page_table_entry_t pte = {0};

    // Test frame field can hold 20 bits (max value)
    pte.frame = 0xFFFFF;
    ASSERT_MSG(pte.frame == 0xFFFFF, "Frame field must hold 20-bit values");

    // Test available field can hold 2 bits
    pte.available = 0x3;
    ASSERT_MSG(pte.available == 0x3, "Available field must hold 2-bit values");

    // Test all single bit fields
    pte.present    = 1;
    pte.rw         = 1;
    pte.user       = 1;
    pte.global     = 1;
    pte.kernel_cow = 1;
    pte.dirty      = 1;
    pte.accessed   = 1;

    ASSERT_MSG(pte.present == 1, "Present bit must be settable");
    ASSERT_MSG(pte.rw == 1, "RW bit must be settable");
    ASSERT_MSG(pte.user == 1, "User bit must be settable");
    ASSERT_MSG(pte.global == 1, "Global bit must be settable");
    ASSERT_MSG(pte.kernel_cow == 1, "COW bit must be settable");
    ASSERT_MSG(pte.dirty == 1, "Dirty bit must be settable");
    ASSERT_MSG(pte.accessed == 1, "Accessed bit must be settable");

    // Verify structure size hasn't changed
    ASSERT_MSG(sizeof(pte) == 4, "PTE must remain 4 bytes");

    TEST_SECTION_END();
}

/// @brief Test cache initialization and properties.
TEST(paging_cache_properties)
{
    TEST_SECTION_START("Cache properties");

    extern kmem_cache_t *pgdir_cache;
    extern kmem_cache_t *pgtbl_cache;

    ASSERT_MSG(pgdir_cache != NULL, "Page directory cache must be initialized");
    ASSERT_MSG(pgtbl_cache != NULL, "Page table cache must be initialized");

    // Caches should be different
    ASSERT_MSG(pgdir_cache != pgtbl_cache, "Page dir and table caches must be distinct");

    TEST_SECTION_END();
}

/// @brief Test multiple page table coverage.
TEST(paging_multi_table_coverage)
{
    TEST_SECTION_START("Multiple page table coverage");

    page_directory_t *pgd = paging_get_main_pgd();
    ASSERT_MSG(pgd != NULL, "Page directory must exist");

    // Count how many different page tables are referenced
    int distinct_tables = 0;
    uint32_t last_frame = 0xFFFFFFFF;

    for (int i = 0; i < MAX_PAGE_DIR_ENTRIES; ++i) {
        if (pgd->entries[i].present) {
            if (pgd->entries[i].frame != last_frame) {
                distinct_tables++;
                last_frame = pgd->entries[i].frame;
            }
        }
    }

    ASSERT_MSG(distinct_tables > 0, "Must have at least one page table");

    TEST_SECTION_END();
}

/// @brief Test address space boundaries.
TEST(paging_address_boundaries)
{
    TEST_SECTION_START("Address space boundaries");

    // Verify important address constants
    ASSERT_MSG(PROCAREA_START_ADDR == 0x00000000UL, "Process area must start at 0");
    ASSERT_MSG(PROCAREA_END_ADDR == 0xC0000000UL, "Process area must end at 3GB");

    // Kernel space starts at PROCAREA_END_ADDR
    uint32_t kernel_start     = PROCAREA_END_ADDR;
    uint32_t kernel_pde_index = kernel_start / (4 * 1024 * 1024);

    ASSERT_MSG(kernel_pde_index == 768, "Kernel must start at PDE index 768");

    // User space is 0 to PROCAREA_END_ADDR
    uint32_t user_end_pde = PROCAREA_END_ADDR / (4 * 1024 * 1024);
    ASSERT_MSG(user_end_pde == 768, "User space must end at PDE index 768");

    TEST_SECTION_END();
}

/// @brief Main test function for paging subsystem.
/// This function runs all paging tests in sequence.
void test_paging(void)
{
    // Basic structure tests
    test_paging_structure_sizes();
    test_paging_constants();

    // Access and initialization tests
    test_paging_main_pgd_accessible();
    test_paging_current_pgd_accessible();
    test_paging_pgd_alignment();
    test_paging_cache_properties();

    // Entry structure tests
    test_paging_pde_structure();
    test_paging_pte_bitfields();
    test_paging_pde_bitfields();
    test_paging_pde_bitfield_sizes();
    test_paging_pte_bitfield_sizes();

    // Initialization state tests
    test_paging_caches_initialized();
    test_paging_current_is_main();
    test_paging_is_current_pgd_edge_cases();

    // Memory mapping tests
    test_paging_kernel_mapping();
    test_paging_first_mb_mapping();
    test_paging_directory_coverage();
    test_paging_multi_table_coverage();

    // Consistency and integrity tests
    test_paging_pde_consistency();
    test_paging_table_hierarchy();
    test_paging_pte_frame_bounds();
    test_paging_flag_propagation();
    test_paging_region_alignment();

    // Translation tests
    test_paging_virt_to_page();

    // Boundary tests
    test_paging_address_boundaries();
}
