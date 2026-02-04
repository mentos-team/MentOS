/// @file page.h
/// @brief Defines the page structure.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "klib/stdatomic.h"
#include "mem/alloc/buddy_system.h"
#include "mem/alloc/slab.h"

/// @brief Page descriptor. Use as a bitmap to understand the order of the block
/// and if it is free or allocated.
typedef struct page {
    /// @brief Array of flags encoding also the zone number to which the page
    /// frame belongs.
    unsigned long flags;
    /// @brief Page frame’s reference counter. 0 free, 1 used, 2+ copy on write
    atomic_t count;
    /// @brief Buddy system page definition
    bb_page_t bbpage;
    /// @brief Contains pointers to the slabs doubly linked list of pages.
    list_head_t slabs;
    /// @brief Slab allocator variables / Contains the total number of objects
    /// in this page, 0 if not managed by the slub.
    unsigned int slab_objcnt;
    /// @brief Tracks the number of free objects in the current page
    unsigned int slab_objfree;
    /// @brief Holds the first free object (if slab_objfree is > 0)
    list_head_t slab_freelist;
    /// @brief This union can either contain the pointer to the slab main page
    /// that handles this page, or the cache that contains it.
    union {
        /// @brief Holds the slab page used to handle this memory region (root
        /// page).
        struct page *slab_main_page;
        /// @brief Holds the slab cache pointer on the main page.
        kmem_cache_t *slab_cache;
    } container;
} page_t;

#define page_count(p)        atomic_read(&(p)->count)   ///< Reads the page count.
#define set_page_count(p, v) atomic_set(&(p)->count, v) ///< Sets the page count.
#define page_inc(p)          atomic_inc(&(p)->count)    ///< Increments the counter for the given page.
#define page_dec(p)          atomic_dec(&(p)->count)    ///< Decrements the counter for the given page.

/// @brief Converts a page structure to its corresponding low memory virtual address.
/// @param page Pointer to the page structure.
/// @return The low memory virtual address corresponding to the specified page,
/// or 0 if the input page pointer is invalid.
uint32_t get_virtual_address_from_page(page_t *page);

/// @brief Converts a page structure to its corresponding physical address.
/// @param page Pointer to the page structure.
/// @return The physical address corresponding to the specified page, or 0 if
/// the input page pointer is invalid.
uint32_t get_physical_address_from_page(page_t *page);

/// @brief Retrieves the page structure corresponding to a given physical address.
/// @param paddr The physical address for which the page structure is requested.
/// @return A pointer to the corresponding page structure, or NULL if the address is invalid.
page_t *get_page_from_physical_address(uint32_t paddr);

/// @brief Retrieves the low memory page corresponding to the given virtual address.
/// @param vaddr the virtual address to convert.
/// @return A pointer to the corresponding page, or NULL if the address is out of range.
page_t *get_page_from_virtual_address(uint32_t vaddr);

/// @brief Enables or disables bootstrap linear mapping for page translations.
/// @param enabled Set to 1 to use bootstrap mapping, 0 to use zone mapping.
void page_set_bootstrap_mapping(int enabled);
