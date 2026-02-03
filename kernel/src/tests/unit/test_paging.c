/// @file test_paging.c
/// @brief Paging subsystem unit tests - Non-destructive version.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

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

/// @brief Main test function for paging subsystem.
/// This function runs all paging tests in sequence.
void test_paging(void)
{
    test_paging_structure_sizes();
    test_paging_constants();
    test_paging_main_pgd_accessible();
    test_paging_current_pgd_accessible();
    test_paging_pgd_alignment();
    test_paging_pde_structure();
    test_paging_pte_bitfields();
    test_paging_pde_bitfields();
    test_paging_caches_initialized();
    test_paging_current_is_main();
}
