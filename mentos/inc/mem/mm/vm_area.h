/// @file vm_area.h
/// @brief Segment-level VMA management.
/// @copyright (c) 2014-2025 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list_head.h"
#include "stdint.h"

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

/// @brief Virtual Memory Area, used to store details of a process segment.
typedef struct vm_area_struct {
    /// Pointer to the memory descriptor associated with this area.
    struct mm_struct *vm_mm;
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

/// @brief Initialize the virtual memory area subsystem.
/// @return 0 on success, -1 on error.
int vm_area_init(void);

/// @brief Create a virtual memory area.
/// @param mm The memory descriptor which will contain the new segment.
/// @param vm_start The virtual address to map to.
/// @param size The size of the segment.
/// @param pgflags The flags for the new memory area.
/// @param gfpflags The Get Free Pages flags.
/// @return The newly created virtual memory area descriptor.
vm_area_struct_t *
vm_area_create(struct mm_struct *mm, uint32_t vm_start, size_t size, uint32_t pgflags, uint32_t gfpflags);

/// @brief Clone a virtual memory area, using copy on write if specified
/// @param mm the memory descriptor which will contain the new segment.
/// @param area the area to clone
/// @param cow whether to use copy-on-write or just copy everything.
/// @param gfpflags the Get Free Pages flags.
/// @return Zero on success.
uint32_t vm_area_clone(struct mm_struct *mm, vm_area_struct_t *area, int cow, uint32_t gfpflags);

/// @brief Destroys a virtual memory area.
/// @param mm the memory descriptor from which we will destroy the area.
/// @param area the are we want to destroy.
/// @return 0 if the area was destroyed, or -1 if the operation failed.
int vm_area_destroy(struct mm_struct *mm, vm_area_struct_t *area);

/// @brief Checks if the given virtual memory area range is valid.
/// @param mm the memory descriptor which we use to check the range.
/// @param vm_start the starting address of the area.
/// @param vm_end the ending address of the area.
/// @return 1 if it's valid, 0 if it's occupied, -1 if it's outside the memory.
int vm_area_is_valid(struct mm_struct *mm, uintptr_t vm_start, uintptr_t vm_end);

/// @brief Searches for the virtual memory area at the given address.
/// @param mm the memory descriptor which should contain the area.
/// @param vm_start the starting address of the area we are looking for.
/// @return a pointer to the area if we found it, NULL otherwise.
vm_area_struct_t *vm_area_find(struct mm_struct *mm, uint32_t vm_start);

/// @brief Searches for an empty spot for a new virtual memory area.
/// @param mm the memory descriptor which should contain the new area.
/// @param length the size of the empty spot.
/// @param vm_start where we save the starting address for the new area.
/// @return 0 on success, -1 on error, or 1 if no free area is found.
int vm_area_search_free_area(struct mm_struct *mm, size_t length, uintptr_t *vm_start);

/// @brief Comparison function between virtual memory areas.
/// @param vma0 Pointer to the first vm_area_struct's list_head_t.
/// @param vma1 Pointer to the second vm_area_struct's list_head_t.
/// @return 1 if vma0 starts after vma1 ends, 0 otherwise.
int vm_area_compare(const list_head_t *vma0, const list_head_t *vma1);
