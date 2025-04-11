/// @file paging.h
/// @brief Implementation of a memory paging management.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "boot.h"
#include "kernel.h"
#include "math.h"
#include "mem/alloc/zone_allocator.h"
#include "mem/mm/mm.h"
#include "mem/mm/vm_area.h"
#include "proc_access.h"
#include "stddef.h"
#include "stdint.h"
#include "sys/bitops.h"

/// 4KB pages (2^12 = 4096 bytes)
#define PAGE_SHIFT  12UL
/// Size of a page (4096 bytes).
#define PAGE_SIZE   (1UL << PAGE_SHIFT)
/// Maximum number of physical page frame numbers (PFNs).
#define MAX_PHY_PFN (1UL << (32UL - PAGE_SHIFT))

/// The start of the process area.
#define PROCAREA_START_ADDR 0x00000000UL
/// The end of the process area (and start of the kernel area).
#define PROCAREA_END_ADDR   0xC0000000UL

/// For a single page table in a 32-bit system.
#define MAX_PAGE_TABLE_ENTRIES 1024
/// For a page directory with 1024 entries.
#define MAX_PAGE_DIR_ENTRIES   1024

/// @brief An entry of a page directory.
typedef struct page_dir_entry {
    unsigned int present : 1;   ///< Page is present in memory.
    unsigned int rw : 1;        ///< Read/write permission (0 = read-only, 1 = read/write).
    unsigned int user : 1;      ///< User/supervisor (0 = supervisor, 1 = user).
    unsigned int w_through : 1; ///< Write-through caching enabled.
    unsigned int cache : 1;     ///< Cache disabled.
    unsigned int accessed : 1;  ///< Page has been accessed.
    unsigned int reserved : 1;  ///< Reserved.
    unsigned int page_size : 1; ///< Page size (0 = 4 KB, 1 = 4 MB).
    unsigned int global : 1;    ///< Global page (not flushed by TLB).
    unsigned int available : 3; ///< Available for system use.
    unsigned int frame : 20;    ///< Frame address (shifted right 12 bits).
} page_dir_entry_t;

/// @brief An entry of a page table.
typedef struct page_table_entry {
    unsigned int present : 1;    ///< Page is present in memory.
    unsigned int rw : 1;         ///< Read/write permission (0 = read-only, 1 = read/write).
    unsigned int user : 1;       ///< User/supervisor (0 = supervisor, 1 = user).
    unsigned int w_through : 1;  ///< Write-through caching enabled.
    unsigned int cache : 1;      ///< Cache disabled.
    unsigned int accessed : 1;   ///< Page has been accessed.
    unsigned int dirty : 1;      ///< Page has been written to.
    unsigned int zero : 1;       ///< Reserved (set to 0).
    unsigned int global : 1;     ///< Global page (not flushed by TLB).
    unsigned int kernel_cow : 1; ///< Kernel copy-on-write.
    unsigned int available : 2;  ///< Available for system use.
    unsigned int frame : 20;     ///< Frame address (shifted right 12 bits).
} page_table_entry_t;

/// @brief A page table.
/// @details
/// It contains 1024 entries which can be addressed by 10 bits (log_2(1024)).
typedef struct page_table {
    /// @brief Array of page table entries.
    page_table_entry_t pages[MAX_PAGE_TABLE_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) page_table_t;

/// @brief A page directory.
/// @details In the two-level paging, this is the first level.
typedef struct page_directory {
    /// @brief Array of page directory entries.
    /// @details
    /// We need a table that contains virtual addresses, so that we can actually
    /// get to the tables (size: 1024 * 4 = 4096 bytes).
    page_dir_entry_t entries[MAX_PAGE_DIR_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) page_directory_t;

/// Cache for storing page directories.
extern kmem_cache_t *pgdir_cache;
/// Cache for storing page tables.
extern kmem_cache_t *pgtbl_cache;

/// @brief Initializes the paging system, sets up memory caches, page
/// directories, and maps important memory regions.
/// @param info   Pointer to the boot information structure, containing kernel
/// addresses and other details.
/// @return 0 on success, -1 on error.
int paging_init(boot_info_t *info);

/// @brief Provide access to the main page directory.
/// @return A pointer to the main page directory, or NULL if main_mm is not initialized.
page_directory_t *paging_get_main_directory(void);

/// @brief Provide access to the current paging directory.
/// @return A pointer to the current page directory.
static inline page_directory_t *paging_get_current_directory(void) { return (page_directory_t *)get_cr3(); }

/// @brief Switches paging directory, the pointer must be a physical address.
/// @param dir A pointer to the new page directory.
static inline void paging_switch_directory(page_directory_t *dir) { set_cr3((uintptr_t)dir); }

/// @brief Checks if the given page directory is the current one.
/// @param pgd A pointer to the page directory to check.
/// @return 1 if the given page directory is the current one, 0 otherwise.
int is_current_pgd(page_directory_t *pgd);

/// @brief Switches paging directory, the pointer can be a lowmem address.
/// @param dir A pointer to the new page directory.
/// @return Returns 0 on success, or -1 if an error occurs.
int paging_switch_directory_va(page_directory_t *dir);

/// @brief Invalidate a single tlb page (the one that maps the specified virtual address)
/// @param addr The address of the page table.
void paging_flush_tlb_single(unsigned long addr);

/// @brief Enables paging.
static inline void paging_enable(void)
{
    // Clear the PSE bit from cr4.
    set_cr4(bitmask_clear(get_cr4(), CR4_PSE));
    // Set the PG bit in cr0.
    set_cr0(bitmask_set(get_cr0(), CR0_PG));
}

/// @brief Returns if paging is enabled.
/// @return 1 if paging is enables, 0 otherwise.
static inline int paging_is_enabled(void) { return bitmask_check(get_cr0(), CR0_PG); }

/// @brief Handles a page fault.
/// @param f The interrupt stack frame.
void page_fault_handler(pt_regs_t *f);

/// @brief Maps a virtual address to a corresponding physical page.
/// @param pgdir The page directory.
/// @param virt_start The starting virtual address to map.
/// @param size Pointer to a size_t variable to store the size of the mapped memory.
/// @return A pointer to the physical page corresponding to the virtual address, or NULL on error.
page_t *mem_virtual_to_page(page_directory_t *pgdir, uint32_t virt_start, size_t *size);

/// @brief Updates the virtual memory area in a page directory.
/// @param pgd The page directory to update.
/// @param virt_start The starting virtual address to update.
/// @param phy_start The starting physical address to map to the virtual addresses.
/// @param size The size of the memory area to update.
/// @param flags Flags to set for the page table entries.
/// @return 0 on success, or -1 on failure.
int mem_upd_vm_area(page_directory_t *pgd, uint32_t virt_start, uint32_t phy_start, size_t size, uint32_t flags);

/// @brief Clones a range of pages between two distinct page tables
/// @param src_pgd   The source page directory.
/// @param dst_pgd   The dest page directory.
/// @param src_start The source virtual address for the clone.
/// @param dst_start The destination virtual address for the clone.
/// @param size      The size of the segment.
/// @param flags     The flags for the new dst memory range.
/// @return 0 on success, -1 on failure.
int mem_clone_vm_area(
    page_directory_t *src_pgd,
    page_directory_t *dst_pgd,
    uint32_t src_start,
    uint32_t dst_start,
    size_t size,
    uint32_t flags);
