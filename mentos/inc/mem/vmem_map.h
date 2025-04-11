/// @file vmem_map.h
/// @brief Virtual memory mapping routines.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "mem/alloc/buddy_system.h"
#include "mem/alloc/zone_allocator.h"
#include "mem/paging.h"

/// @brief Virtual mapping manager.
typedef struct virt_map_page_manager {
    /// The buddy system used to manage the pages.
    bb_instance_t bb_instance;
} virt_map_page_manager_t;

/// @brief Virtual mapping.
typedef struct virt_map_page {
    /// A buddy system page.
    bb_page_t bbpage;
} virt_map_page_t;

/// @brief Initialize the virtual memory mapper.
/// @return Returns 0 on success, or -1 if an error occurs.
int virt_init(void);

/// @brief Maps physical pages to virtual memory.
/// @param page Pointer to the physical page.
/// @param pfn_count The number of page frames to map.
/// @return The virtual address of the mapped pages, or 0 on failure.
uint32_t virt_map_physical_pages(page_t *page, int pfn_count);

/// @brief Allocates virtual pages for a given size.
/// @param size The size in bytes to allocate.
/// @return Pointer to the allocated virtual pages, or NULL on failure.
virt_map_page_t *virt_map_alloc(uint32_t size);

/// @brief Maps a virtual address to a virtual memory area.
/// @param mm Pointer to the memory management structure.
/// @param vpage Pointer to the virtual map page.
/// @param vaddr The virtual address to map.
/// @param size The size of the memory area to map.
/// @return The starting virtual address of the mapped area, or 0 on failure.
uint32_t virt_map_vaddress(mm_struct_t *mm, virt_map_page_t *vpage, uint32_t vaddr, uint32_t size);

/// @brief Unmaps a virtual address from the virtual memory.
/// @param addr The virtual address to unmap.
/// @return Returns 0 on success, or -1 if an error occurs.
int virt_unmap(uint32_t addr);

/// @brief Unmap a page.
/// @param page Pointer to the page to unmap.
/// @return Returns 0 on success, or -1 if an error occurs.
int virt_unmap_pg(virt_map_page_t *page);

/// @brief Memcpy from different processes virtual addresses
/// @param dst_mm The destination memory struct
/// @param dst_vaddr The destination memory address
/// @param src_mm The source memory struct
/// @param src_vaddr The source memory address
/// @param size The size in bytes of the copy
void virt_memcpy(mm_struct_t *dst_mm, uint32_t dst_vaddr, mm_struct_t *src_mm, uint32_t src_vaddr, uint32_t size);
