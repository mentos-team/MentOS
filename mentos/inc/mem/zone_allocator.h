/// @file zone_allocator.h
/// @brief Implementation of the Zone Allocator
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "mem/gfp.h"
#include "math.h"
#include "stdint.h"
#include "klib/list_head.h"
#include "sys/bitops.h"
#include "klib/stdatomic.h"
#include "boot.h"
#include "mem/buddysystem.h"
#include "mem/slab.h"

#define page_count(p)        atomic_read(&(p)->count)   ///< Reads the page count.
#define set_page_count(p, v) atomic_set(&(p)->count, v) ///< Sets the page count.
#define page_inc(p)          atomic_inc(&(p)->count)    ///< Increments the counter for the given page.
#define page_dec(p)          atomic_dec(&(p)->count)    ///< Decrements the counter for the given page.

/// @brief Page descriptor. Use as a bitmap to understand the order of the block
/// and if it is free or allocated.
typedef struct page_t {
    /// @brief Array of flags encoding also the zone number to which the page
    /// frame belongs.
    unsigned long flags;
    /// @brief Page frame’s reference counter. 0 free, 1 used, 2+ copy on write
    atomic_t count;
    /// @brief Buddy system page definition
    bb_page_t bbpage;
    /// @brief Contains pointers to the slabs doubly linked list of pages.
    list_head slabs;

    /// @brief Slab allocator variables / Contains the total number of objects
    /// in this page, 0 if not managed by the slub.
    unsigned int slab_objcnt;
    /// @brief Tracks the number of free objects in the current page
    unsigned int slab_objfree;
    /// @brief Holds the first free object (if slab_objfree is > 0)
    list_head slab_freelist;
    /// @brief This union can either contain the pointer to the slab main page
    /// that handles this page, or the cache that contains it.
    union {
        /// @brief Holds the slab page used to handle this memory region (root
        /// page).
        struct page_t *slab_main_page;
        /// @brief Holds the slab cache pointer on the main page.
        kmem_cache_t *slab_cache;
    } container;
} page_t;

/// @brief Enumeration for zone_t.
enum zone_type {
    /// @brief Direct mapping. Used by the kernel.
    /// @details
    /// Normal addressable memory is in **ZONE_NORMAL**. DMA operations can be
    /// performed on pages in **ZONE_NORMAL** if the DMA devices support
    /// transfers to all addressable memory.
    ZONE_NORMAL,

    /// @brief Page tables mapping. Used by user processes.
    /// @details
    /// A memory area that is only addressable by the kernel through mapping
    /// portions into its own address space. This is for example used by i386 to
    /// allow the kernel to address the memory beyond 900MB. The kernel will set
    /// up special mappings (page table entries on i386) for each page that the
    /// kernel needs to access.
    ZONE_HIGHMEM,

    /// The maximum number of zones.
    __MAX_NR_ZONES
};

/// @brief Data structure to differentiate memory zone.
typedef struct zone_t {
    /// Number of free pages in the zone.
    unsigned long free_pages;
    /// Buddy system managing this zone
    bb_instance_t buddy_system;
    /// Pointer to first page descriptor of the zone.
    page_t *zone_mem_map;
    /// Index of the first page frame of the zone.
    uint32_t zone_start_pfn;
    /// Zone's name.
    char *name;
    /// Zone's size in number of pages.
    unsigned long size;
} zone_t;

/// @brief Data structure to rapresent a memory node. In Uniform memory access
/// (UMA) architectures there is only one node called contig_page_data.
typedef struct pg_data_t {
    /// Zones of the node.
    zone_t node_zones[__MAX_NR_ZONES];
    /// Number of zones in the node.
    int nr_zones;
    /// Array of pages of the node.
    page_t *node_mem_map;
    /// Physical address of the first page of the node.
    unsigned long node_start_paddr;
    /// Index on global mem_map for node_mem_map.
    unsigned long node_start_mapnr;
    /// Node's size in number of pages.
    unsigned long node_size;
    /// NID.
    int node_id;
    /// Next item in the memory node list.
    struct pg_data_t *node_next;
} pg_data_t;

extern page_t *mem_map;
extern pg_data_t *contig_page_data;

/// @brief Find the nearest block's order of size greater than the amount of
/// byte.
/// @param base_addr The start address, used to handle extra page calculation in
/// case of not page aligned addresses.
/// @param amount    The amount of byte which we want to calculate order.
/// @return The block's order greater and nearest than amount.
uint32_t find_nearest_order_greater(uint32_t base_addr, uint32_t amount);

/// @brief Physical memory manager initialization.
/// @param boot_info Information coming from the booloader.
/// @return Outcome of the operation.
int pmmngr_init(boot_info_t *boot_info);

/// @brief Alloc a single cached page.
/// @param gfp_mask The GetFreePage mask.
/// @return Pointer to the page.
page_t *alloc_page_cached(gfp_t gfp_mask);

/// @brief Free a page allocated with alloc_page_cached.
/// @param page Pointer to the page to free.
void free_page_cached(page_t *page);

/// @brief Find the first free page frame, set it allocated and return the
/// memory address of the page frame.
/// @param gfp_mask GFP_FLAGS to decide the zone allocation.
/// @return Memory address of the first free block.
uint32_t __alloc_page_lowmem(gfp_t gfp_mask);

/// @brief Frees the given page frame address.
/// @param addr The block address.
void free_page_lowmem(uint32_t addr);

/// @brief Find the first free 2^order amount of page frames, set it allocated
/// and return the memory address of the first page frame allocated.
/// @param gfp_mask GFP_FLAGS to decide the zone allocation.
/// @param order    The logarithm of the size of the page frame.
/// @return Memory address of the first free page frame allocated.
uint32_t __alloc_pages_lowmem(gfp_t gfp_mask, uint32_t order);

/// @brief Find the first free 2^order amount of page frames, set it allocated
/// and return the memory address of the first page frame allocated.
/// @param gfp_mask GFP_FLAGS to decide the zone allocation.
/// @param order    The logarithm of the size of the page frame.
/// @return Memory address of the first free page frame allocated.
page_t *_alloc_pages(gfp_t gfp_mask, uint32_t order);

/// @brief Get the start address of the corresponding page.
/// @param page A page structure.
/// @return The address that corresponds to the page.
uint32_t get_lowmem_address_from_page(page_t *page);

/// @brief Get the start physical address of the corresponding page.
/// @param page A page structure
/// @return The physical address that corresponds to the page.
uint32_t get_physical_address_from_page(page_t *page);

/// @brief Get the page from it's physical address.
/// @param phy_addr The physical address
/// @return The page that corresponds to the physical address.
page_t *get_page_from_physical_address(uint32_t phy_addr);

/// @brief Get the page that contains the specified address.
/// @param addr A phisical address.
/// @return The page that corresponds to the address.
page_t *get_lowmem_page_from_address(uint32_t addr);

/// @brief Frees from the given page frame address up to 2^order amount of page
/// frames.
/// @param addr The page frame address.
void free_pages_lowmem(uint32_t addr);

/// @brief Frees from the given page frame address up to 2^order amount of page
/// frames.
/// @param page The page.
void __free_pages(page_t *page);

/// @brief Returns the total space for the given zone.
/// @param gfp_mask GFP_FLAGS to decide the zone.
/// @return Total space of the given zone.
unsigned long get_zone_total_space(gfp_t gfp_mask);

/// @brief Returns the total free space for the given zone.
/// @param gfp_mask GFP_FLAGS to decide the zone.
/// @return Total free space of the given zone.
unsigned long get_zone_free_space(gfp_t gfp_mask);

/// @brief Returns the total cached space for the given zone.
/// @param gfp_mask GFP_FLAGS to decide the zone.
/// @return Total cached space of the given zone.
unsigned long get_zone_cached_space(gfp_t gfp_mask);

/// @brief Checks if the specified address points to a page_t (or field) that
/// belongs to lowmem.
/// @param addr The address to check.
/// @return 1 if it belongs to lowmem, 0 otherwise.
static inline int is_lowmem_page_struct(void *addr)
{
    uint32_t start_lowm_map  = (uint32_t)contig_page_data->node_zones[ZONE_NORMAL].zone_mem_map;
    uint32_t lowmem_map_size = sizeof(page_t) * contig_page_data->node_zones[ZONE_NORMAL].size;
    uint32_t map_index       = (uint32_t)addr - start_lowm_map;
    return map_index < lowmem_map_size;
}
