/// @file paging.h
/// @brief Implementation of a memory paging management.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "mem/zone_allocator.h"
#include "proc_access.h"
#include "kernel.h"
#include "stddef.h"
#include "boot.h"

/// Size of a page.
#define PAGE_SIZE 4096U
/// The start of the process area.
#define PROCAREA_START_ADDR 0x00000000
/// The end of the process area (and start of the kernel area).
#define PROCAREA_END_ADDR 0xC0000000

/// @brief An entry of a page directory.
typedef struct page_dir_entry_t {
    unsigned int present : 1;   ///< TODO: Comment.
    unsigned int rw : 1;        ///< TODO: Comment.
    unsigned int user : 1;      ///< TODO: Comment.
    unsigned int w_through : 1; ///< TODO: Comment.
    unsigned int cache : 1;     ///< TODO: Comment.
    unsigned int accessed : 1;  ///< TODO: Comment.
    unsigned int reserved : 1;  ///< TODO: Comment.
    unsigned int page_size : 1; ///< TODO: Comment.
    unsigned int global : 1;    ///< TODO: Comment.
    unsigned int available : 3; ///< TODO: Comment.
    unsigned int frame : 20;    ///< TODO: Comment.
} page_dir_entry_t;

/// @brief An entry of a page table.
typedef struct page_table_entry_t {
    unsigned int present : 1;    ///< TODO: Comment.
    unsigned int rw : 1;         ///< TODO: Comment.
    unsigned int user : 1;       ///< TODO: Comment.
    unsigned int w_through : 1;  ///< TODO: Comment.
    unsigned int cache : 1;      ///< TODO: Comment.
    unsigned int accessed : 1;   ///< TODO: Comment.
    unsigned int dirty : 1;      ///< TODO: Comment.
    unsigned int zero : 1;       ///< TODO: Comment.
    unsigned int global : 1;     ///< TODO: Comment.
    unsigned int kernel_cow : 1; ///< TODO: Comment.
    unsigned int available : 2;  ///< TODO: Comment.
    unsigned int frame : 20;     ///< TODO: Comment.
} page_table_entry_t;

/// @brief Flags associated with virtual memory areas.
enum MEMMAP_FLAGS {
    MM_USER    = 0x1, ///< Area belongs to user.
    MM_GLOBAL  = 0x2, ///< Area is global.
    MM_RW      = 0x4, ///< Area has user read/write perm.
    MM_PRESENT = 0x8, ///< Area is valid.
    // Kernel flags
    MM_COW     = 0x10, ///< Area is copy on write.
    MM_UPDADDR = 0x20, ///< Check?
};

/// @brief A page table.
/// @details
/// It contains 1024 entries which can be addressed by 10 bits (log_2(1024)).
typedef struct page_table_t {
    page_table_entry_t pages[1024]; ///< Array of pages.
} __attribute__((aligned(PAGE_SIZE))) page_table_t;

/// @brief A page directory.
/// @details In the two-level paging, this is the first level.
typedef struct page_directory_t {
    /// We need a table that contains virtual address, so that we can
    /// actually get to the tables (size: 1024 * 4 = 4096 byte).
    page_dir_entry_t entries[1024];
} __attribute__((aligned(PAGE_SIZE))) page_directory_t;

/// @brief Virtual Memory Area, used to store details of a process segment.
typedef struct vm_area_struct_t {
    /// Memory descriptor associated.
    struct mm_struct_t *vm_mm;
    /// Start address of the segment, inclusive.
    uint32_t vm_start;
    /// End address of the segment, exclusive.
    uint32_t vm_end;
    /// List of memory areas.
    list_head vm_list;
    /// Permissions.
    pgprot_t vm_page_prot;
    /// Flags.
    unsigned short vm_flags;
    /// rbtree node.
    // struct rb_node vm_rb;
} vm_area_struct_t;

/// @brief Memory Descriptor, used to store details about the memory of a user process.
typedef struct mm_struct_t {
    /// List of memory area (vm_area_struct reference).
    list_head mmap_list;
    // /// rbtree of memory area.
    // struct rb_root mm_rb;
    /// Last memory area used.
    vm_area_struct_t *mmap_cache;
    /// Process page directory.
    page_directory_t *pgd;
    /// Number of memory area.
    int map_count;
    /// List of mm_struct.
    list_head mm_list;
    /// CODE start.
    uint32_t start_code;
    /// CODE end.
    uint32_t end_code;
    /// DATA start.
    uint32_t start_data;
    /// DATA end.
    uint32_t end_data;
    /// HEAP start.
    uint32_t start_brk;
    /// HEAP end.
    uint32_t brk;
    /// STACK start.
    uint32_t start_stack;
    /// ARGS start.
    uint32_t arg_start;
    /// ARGS end.
    uint32_t arg_end;
    /// ENVIRONMENT start.
    uint32_t env_start;
    /// ENVIRONMENT end.
    uint32_t env_end;
    /// Number of mapped pages.
    unsigned int total_vm;
} mm_struct_t;

/// @brief Cache used to store page tables.
extern kmem_cache_t *pgtbl_cache;

/// @brief Initializes paging
/// @param info Information coming from bootloader.
void paging_init(boot_info_t *info);

/// @brief Provide access to the main page directory.
/// @return A pointer to the main page directory.
page_directory_t *paging_get_main_directory();

/// @brief Provide access to the current paging directory.
/// @return A pointer to the current page directory.
static inline page_directory_t *paging_get_current_directory()
{
    return (page_directory_t *)get_cr3();
}

/// @brief Switches paging directory, the pointer must be a physical address.
/// @param dir A pointer to the new page directory.
static inline void paging_switch_directory(page_directory_t *dir)
{
    set_cr3((uintptr_t)dir);
}

/// @brief Switches paging directory, the pointer can be a lowmem address.
/// @param dir A pointer to the new page directory.
void paging_switch_directory_va(page_directory_t *dir);

/// @brief Invalidate a single tlb page (the one that maps the specified virtual address)
/// @param addr The address of the page table.
void paging_flush_tlb_single(unsigned long addr);

/// @brief Enables paging.
static inline void paging_enable()
{
    // Clear the PSE bit from cr4.
    set_cr4(bitmask_clear(get_cr4(), CR4_PSE));
    // Set the PG bit in cr0.
    set_cr0(bitmask_set(get_cr0(), CR0_PG));
}

/// @brief Returns if paging is enabled.
/// @return 1 if paging is enables, 0 otherwise.
static inline int paging_is_enabled()
{
    return bitmask_check(get_cr0(), CR0_PG);
}

/// @brief Handles a page fault.
/// @param f The interrupt stack frame.
void page_fault_handler(pt_regs *f);

/// @brief Gets a page from a virtual address
/// @param pgdir      The target page directory.
/// @param virt_start The virtual address to query
/// @param size       A pointer to the requested size of the data, size is updated if physical memory is not contiguous
/// @return Pointer to the page.
page_t *mem_virtual_to_page(page_directory_t *pgdir, uint32_t virt_start, size_t *size);

/// @brief Creates a virtual to physical mapping, incrementing pages usage counters.
/// @param pgd        The target page directory.
/// @param virt_start The virtual address to map to.
/// @param phy_start  The physical address to map.
/// @param size       The size of the segment.
/// @param flags      The flags for the memory range.
void mem_upd_vm_area(page_directory_t *pgd, uint32_t virt_start, uint32_t phy_start, size_t size, uint32_t flags);

/// @brief Clones a range of pages between two distinct page tables
/// @param src_pgd   The source page directory.
/// @param dst_pgd   The dest page directory.
/// @param src_start The source virtual address for the clone.
/// @param dst_start The destination virtual address for the clone.
/// @param size      The size of the segment.
/// @param flags     The flags for the new dst memory range.
void mem_clone_vm_area(page_directory_t *src_pgd,
                       page_directory_t *dst_pgd,
                       uint32_t src_start,
                       uint32_t dst_start,
                       size_t size,
                       uint32_t flags);

/// @brief Create a virtual memory area.
/// @param mm         The memory descriptor which will contain the new segment.
/// @param virt_start The virtual address to map to.
/// @param size       The size of the segment.
/// @param pgflags    The flags for the new memory area.
/// @param gfpflags   The Get Free Pages flags.
/// @return The virtual address of the starting point of the segment.
uint32_t create_vm_area(mm_struct_t *mm,
                        uint32_t virt_start,
                        size_t size,
                        uint32_t pgflags,
                        uint32_t gfpflags);

/// @brief Clone a virtual memory area, using copy on write if specified
/// @param mm       The memory descriptor which will contain the new segment.
/// @param area     The area to clone
/// @param cow      Whether to use copy-on-write or just copy everything.
/// @param gfpflags The Get Free Pages flags.
/// @return Zero on success.
uint32_t clone_vm_area(mm_struct_t *mm,
                       vm_area_struct_t *area,
                       int cow,
                       uint32_t gfpflags);

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
void destroy_process_image(mm_struct_t *mm);
