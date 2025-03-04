/// @file paging.h
/// @brief Implementation of a memory paging management.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "boot.h"
#include "kernel.h"
#include "mem/zone_allocator.h"
#include "proc_access.h"
#include "stddef.h"
#include "stdint.h"

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
typedef struct page_dir_entry_t {
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
typedef struct page_table_entry_t {
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

/// @brief Flags associated with virtual memory areas.
enum MEMMAP_FLAGS {
    MM_USER    = 0x1, ///< Area belongs to user mode (accessible by user-level processes).
    MM_GLOBAL  = 0x2, ///< Area is global (not flushed from TLB on context switch).
    MM_RW      = 0x4, ///< Area has read/write permissions.
    MM_PRESENT = 0x8, ///< Area is present in memory.
    // Kernel flags
    MM_COW     = 0x10, ///< Area is copy-on-write (used for forked processes).
    MM_UPDADDR = 0x20, ///< Update address (used for special memory mappings).
};

/// @brief A page table.
/// @details
/// It contains 1024 entries which can be addressed by 10 bits (log_2(1024)).
typedef struct page_table_t {
    /// @brief Array of page table entries.
    page_table_entry_t pages[MAX_PAGE_TABLE_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) page_table_t;

/// @brief A page directory.
/// @details In the two-level paging, this is the first level.
typedef struct page_directory_t {
    /// @brief Array of page directory entries.
    /// @details
    /// We need a table that contains virtual addresses, so that we can actually
    /// get to the tables (size: 1024 * 4 = 4096 bytes).
    page_dir_entry_t entries[MAX_PAGE_DIR_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) page_directory_t;

/// @brief Virtual Memory Area, used to store details of a process segment.
typedef struct vm_area_struct_t {
    /// Pointer to the memory descriptor associated with this area.
    struct mm_struct_t *vm_mm;
    /// Start address of the segment, inclusive.
    uint32_t vm_start;
    /// End address of the segment, exclusive.
    uint32_t vm_end;
    /// Linked list of memory areas.
    list_head_t vm_list;
    /// Page protection flags (permissions).
    pgprot_t vm_page_prot;
    /// Flags indicating attributes of the memory area.
    unsigned short vm_flags;
} vm_area_struct_t;

/// @brief Memory Descriptor, used to store details about the memory of a user process.
typedef struct mm_struct_t {
    /// List of memory areas (vm_area_struct references).
    list_head_t mmap_list;
    /// Pointer to the last used memory area.
    vm_area_struct_t *mmap_cache;
    /// Pointer to the process's page directory.
    page_directory_t *pgd;
    /// Number of memory areas.
    int map_count;
    /// List of mm_structs.
    list_head_t mm_list;
    /// Start address of the code segment.
    uint32_t start_code;
    /// End address of the code segment.
    uint32_t end_code;
    /// Start address of the data segment.
    uint32_t start_data;
    /// End address of the data segment.
    uint32_t end_data;
    /// Start address of the heap.
    uint32_t start_brk;
    /// End address of the heap.
    uint32_t brk;
    /// Start address of the stack.
    uint32_t start_stack;
    /// Start address of the arguments.
    uint32_t arg_start;
    /// End address of the arguments.
    uint32_t arg_end;
    /// Start address of the environment variables.
    uint32_t env_start;
    /// End address of the environment variables.
    uint32_t env_end;
    /// Total number of mapped pages.
    unsigned int total_vm;
} mm_struct_t;

/// @brief Cache used to store page tables.
extern kmem_cache_t *pgtbl_cache;

/// @brief Comparison function between virtual memory areas.
/// @param vma0 Pointer to the first vm_area_struct's list_head_t.
/// @param vma1 Pointer to the second vm_area_struct's list_head_t.
/// @return 1 if vma0 starts after vma1 ends, 0 otherwise.
static inline int vm_area_compare(const list_head_t *vma0, const list_head_t *vma1)
{
    // Retrieve the vm_area_struct from the list_head_t for vma0.
    vm_area_struct_t *_vma0 = list_entry(vma0, vm_area_struct_t, vm_list);
    // Retrieve the vm_area_struct from the list_head_t for vma1.
    vm_area_struct_t *_vma1 = list_entry(vma1, vm_area_struct_t, vm_list);
    // Compare the start address of vma0 with the end address of vma1.
    return _vma0->vm_start > _vma1->vm_end;
}

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
void page_fault_handler(pt_regs *f);

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

/// @brief Create a virtual memory area.
/// @param mm The memory descriptor which will contain the new segment.
/// @param vm_start The virtual address to map to.
/// @param size The size of the segment.
/// @param pgflags The flags for the new memory area.
/// @param gfpflags The Get Free Pages flags.
/// @return The newly created virtual memory area descriptor.
vm_area_struct_t *create_vm_area(mm_struct_t *mm, uint32_t vm_start, size_t size, uint32_t pgflags, uint32_t gfpflags);

/// @brief Clone a virtual memory area, using copy on write if specified
/// @param mm the memory descriptor which will contain the new segment.
/// @param area the area to clone
/// @param cow whether to use copy-on-write or just copy everything.
/// @param gfpflags the Get Free Pages flags.
/// @return Zero on success.
uint32_t clone_vm_area(mm_struct_t *mm, vm_area_struct_t *area, int cow, uint32_t gfpflags);

/// @brief Destroys a virtual memory area.
/// @param mm the memory descriptor from which we will destroy the area.
/// @param area the are we want to destroy.
/// @return 0 if the area was destroyed, or -1 if the operation failed.
int destroy_vm_area(mm_struct_t *mm, vm_area_struct_t *area);

/// @brief Searches for the virtual memory area at the given address.
/// @param mm the memory descriptor which should contain the area.
/// @param vm_start the starting address of the area we are looking for.
/// @return a pointer to the area if we found it, NULL otherwise.
vm_area_struct_t *find_vm_area(mm_struct_t *mm, uint32_t vm_start);

/// @brief Checks if the given virtual memory area range is valid.
/// @param mm the memory descriptor which we use to check the range.
/// @param vm_start the starting address of the area.
/// @param vm_end the ending address of the area.
/// @return 1 if it's valid, 0 if it's occupied, -1 if it's outside the memory.
int is_valid_vm_area(mm_struct_t *mm, uintptr_t vm_start, uintptr_t vm_end);

/// @brief Searches for an empty spot for a new virtual memory area.
/// @param mm the memory descriptor which should contain the new area.
/// @param length the size of the empty spot.
/// @param vm_start where we save the starting address for the new area.
/// @return 0 on success, -1 on error, or 1 if no free area is found.
int find_free_vm_area(mm_struct_t *mm, size_t length, uintptr_t *vm_start);

/// @brief Creates the main memory descriptor.
/// @param stack_size The size of the stack in byte.
/// @return The Memory Descriptor created.
mm_struct_t *create_blank_process_image(size_t stack_size);

/// @brief Create a Memory Descriptor.
/// @param mmp The memory map to clone
/// @return The Memory Descriptor created.
mm_struct_t *clone_process_image(mm_struct_t *mmp);

/// @brief Free Memory Descriptor with all the memory segment contained.
/// @param mm The Memory Descriptor to free.
/// @return Returns -1 on error, otherwise 0.
int destroy_process_image(mm_struct_t *mm);
