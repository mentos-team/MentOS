/// @file buddysystem.h
/// @brief Buddy System.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "klib/list_head.h"
#include "klib/stdatomic.h"
#include "stdint.h"

/// @brief Max gfp pages order of buddysystem blocks.
#define MAX_BUDDYSYSTEM_GFP_ORDER 14

/// @brief Provide the offset of the element inside the given type of page.
#define BBSTRUCT_OFFSET(page, element) \
    ((uint32_t) & (((page *)NULL)->element))

/// @brief Returns the address of the given element of a given type of page,
///        based on the provided bbstruct.
#define PG_FROM_BBSTRUCT(bbstruct, page, element) \
    ((page *)(((uint32_t)(bbstruct)) - BBSTRUCT_OFFSET(page, element)))

/// The base structure representing a bb page
typedef struct bb_page_t {
    /// The flags of the page.
    volatile unsigned long flags;
    /// The current page order.
    uint32_t order;
    /// Keep track of where the page is located.
    union {
        /// The page siblings when not allocated.
        list_head siblings;
        /// The cache list pointer when allocated but on cache.
        list_head cache;
    } location;
} bb_page_t;

/// @brief Buddy system descriptor: collection of free page blocks.
/// Each block represents 2^k free contiguous page.
typedef struct bb_free_area_t {
    /// free_list collectes the first page descriptors of a blocks of 2^k frames
    list_head free_list;
    /// nr_free specifies the number of blocks of free pages.
    int nr_free;
} bb_free_area_t;

/// @brief Buddy system instance,
/// that represents a memory area managed by the buddy system
typedef struct bb_instance_t {
    /// Name of this bb instance
    const char *name;
    /// List of buddy system pages grouped by level.
    bb_free_area_t free_area[MAX_BUDDYSYSTEM_GFP_ORDER];
    /// Pointer to start of free pages cache
    list_head free_pages_cache_list;
    /// Size of the current cache
    unsigned long free_pages_cache_size;
    /// Buddysystem instance size in number of pages.
    unsigned long size;
    /// Address of the first managed page
    bb_page_t *base_page;
    /// Size of the (padded) wrapper page structure
    unsigned long pgs_size;
    /// Offset of the bb_page_t struct from the start of the whole structure
    unsigned long bbpg_offset;
} bb_instance_t;

/// @brief  Allocate a block of page frames of size 2^order.
/// @param instance A buddy system instance.
/// @param order    The logarithm of the size of the block.
/// @return The address of the first page descriptor of the block, or NULL.
bb_page_t *bb_alloc_pages(bb_instance_t *instance, unsigned int order);

/// @brief Free a block of page frames of size 2^order.
/// @param instance A buddy system instance.
/// @param page     The address of the first page descriptor of the block.
void bb_free_pages(bb_instance_t *instance, bb_page_t *page);

/// @brief Alloc a page using bb cache.
/// @param instance Buddy system instance.
/// @return An allocated page.
bb_page_t *bb_alloc_page_cached(bb_instance_t *instance);

/// @brief Free a page allocated with bb_alloc_page_cached.
/// @param instance Buddy system instance.
/// @param page     The address of the first page descriptor of the block.
void bb_free_page_cached(bb_instance_t *instance, bb_page_t *page);

/// @brief Initialize Buddy System.
/// @param instance      A buddysystem instance.
/// @param name          The name of the current instance (for debug purposes)
/// @param pages_start   The start address of the page structures
/// @param bbpage_offset The offset from the start of the whole page of the
///                      bb_page_t struct.
/// @param pages_stride  The (padded) size of the whole page structure
/// @param pages_count   The number of pages in this region
void buddy_system_init(
    bb_instance_t *instance,
    const char *name,
    void *pages_start,
    uint32_t bbpage_offset,
    uint32_t pages_stride,
    uint32_t pages_count);

/// @brief Print the size of free_list of each free_area.
/// @param instance A buddy system instance.
void buddy_system_dump(bb_instance_t *instance);

/// @brief Returns the total space for the given instance.
/// @param instance A buddy system instance.
/// @return The requested total sapce.
unsigned long buddy_system_get_total_space(bb_instance_t *instance);

/// @brief Returns the free space for the given instance.
/// @param instance A buddy system instance.
/// @return The requested total sapce.
unsigned long buddy_system_get_free_space(bb_instance_t *instance);

/// @brief Returns the cached space for the given instance.
/// @param instance A buddy system instance.
/// @return The requested total sapce.
unsigned long buddy_system_get_cached_space(bb_instance_t *instance);
