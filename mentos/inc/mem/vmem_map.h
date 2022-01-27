/// @file vmem_map.h
/// @brief Virtual memory mapping routines.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "mem/buddysystem.h"
#include "mem/zone_allocator.h"
#include "mem/paging.h"

/// Size of the virtual memory.
#define VIRTUAL_MEMORY_SIZE_MB 128

/// @brief Virtual mapping manager.
typedef struct virt_map_page_manager_t {
    /// The buddy system used to manage the pages.
    bb_instance_t bb_instance;
} virt_map_page_manager_t;

/// @brief Virtual mapping.
typedef struct virt_map_page_t {
    /// A buddy system page.
    bb_page_t bbpage;
} virt_map_page_t;

/// @brief Initialize the virtual memory mapper
void virt_init(void);

/// @brief Map a page range to virtual memory
/// @param page The start page of the mapping
/// @param pfn_count The number of pages to map
/// @return The virtual address of the mapping
uint32_t virt_map_physical_pages(page_t *page, int pfn_count);

/// @brief Allocate a virtual page range of the specified size.
/// @param size The required amount.
/// @return Pointer to the allocated memory.
virt_map_page_t *virt_map_alloc(uint32_t size);

/// @brief Map a page to a memory area portion.
/// @param mm    The memory descriptor.
/// @param vpage Pointer to the virtual page.
/// @param vaddr The starting address of the are.
/// @param size  The size of the area.
/// @return Address of the mapped area.
uint32_t virt_map_vaddress(mm_struct_t *mm,
                           virt_map_page_t *vpage,
                           uint32_t vaddr,
                           uint32_t size);

/// @brief Checks if an address belongs to the virtual memory mapping.
/// @param addr The address to check.
/// @return 1 if it belongs to the virtual memory mapping, 0 otherwise.
int virtual_check_address(uint32_t addr);

/// @brief Unmap an address.
/// @param addr The address to unmap.
void virt_unmap(uint32_t addr);

/// @brief Unmap a page.
/// @param page Pointer to the page to unmap.
void virt_unmap_pg(virt_map_page_t *page);

/// @brief Memcpy from different processes virtual addresses
/// @param dst_mm The destination memory struct
/// @param dst_vaddr The destination memory address
/// @param src_mm The source memory struct
/// @param src_vaddr The source memory address
/// @param size The size in bytes of the copy
void virt_memcpy(
    mm_struct_t *dst_mm,
    uint32_t dst_vaddr,
    mm_struct_t *src_mm,
    uint32_t src_vaddr,
    uint32_t size);
