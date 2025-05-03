/// @file zone_allocator.h
/// @brief Implementation of the Zone Allocator
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "boot.h"
#include "mem/mm/page.h"

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
typedef struct zone {
    /// Zone's name.
    char *name;
    /// Pointer to first page descriptor of the zone.
    page_t *zone_mem_map;
    /// Index of the first page frame of the zone.
    uint32_t zone_start_pfn;
    /// Zone's size in number of pages.
    size_t num_pages;
    /// Number of free pages in the zone.
    size_t free_pages;
    /// Total size of the zone.
    size_t total_size;
    /// Buddy system managing this zone
    bb_instance_t buddy_system;
} zone_t;

/// @brief Data structure to rapresent a memory node. In Uniform memory access
/// (UMA) architectures there is only one node called contig_page_data.
typedef struct pg_data {
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

/// @brief Structure to represent a memory zone (LowMem or HighMem).
typedef struct memory_zone {
    uint32_t start_addr; ///< Start address of the zone (physical).
    uint32_t end_addr;   ///< End address of the zone (physical).
    uint32_t virt_start; ///< Virtual start address of the zone.
    uint32_t virt_end;   ///< Virtual end address of the zone.
    uint32_t size;       ///< Total size of the zone in bytes.
} memory_zone_t;

/// @brief Structure to encapsulate system memory management data.
typedef struct memory_info {
    page_t *mem_map;         ///< Pointer to the array of all physical memory blocks.
    pg_data_t *page_data;    ///< Pointer to the contiguous memory node descriptor.
    uint32_t mem_size;       ///< Total size of available physical memory (bytes).
    uint32_t mem_map_num;    ///< Total number of memory frames (pages) available.
    uint32_t page_index_min; ///< Minimum page index.
    uint32_t page_index_max; ///< Maximum page index.
    memory_zone_t low_mem;   ///< Low memory zone (normal zone).
    memory_zone_t high_mem;  ///< High memory zone.
} memory_info_t;

/// @brief Keeps track of system memory management data.
extern memory_info_t memory;

// @brief Checks if a virtual address is valid within the low or high memory zones.
/// @param vaddr The virtual address to be checked.
/// @return 1 if the virtual address is valid, 0 otherwise.
int is_valid_virtual_address(uint32_t vaddr);

/// @brief Finds the nearest order of memory allocation that can accommodate a
/// given amount of memory.
/// @param base_addr the base address from which to calculate the number of
/// pages.
/// @param amount the amount of memory (in bytes) to allocate.
/// @return The nearest order (power of two) that is greater than or equal to
/// the number of pages required.
uint32_t find_nearest_order_greater(uint32_t base_addr, uint32_t amount);

/// @brief Physical memory manager initialization.
/// @param boot_info Information coming from the booloader.
/// @return Outcome of the operation.
int pmmngr_init(boot_info_t *boot_info);

/// @brief Find the first free 2^order amount of page frames, set it allocated
/// and return the memory address of the first page frame allocated.
/// @param file     The file name where the allocation is done.
/// @param func     The function name where the allocation is done.
/// @param line     The line number where the allocation is done.
/// @param gfp_mask GFP_FLAGS to decide the zone allocation.
/// @param order    The logarithm of the size of the page frame.
/// @return Memory address of the first free page frame allocated, or NULL if
/// allocation fails.
page_t *pr_alloc_pages(const char *file, const char *func, int line, gfp_t gfp_mask, uint32_t order);

/// @brief Frees from the given page frame address up to 2^order amount of page
/// frames.
/// @param file The file name where the free is done.
/// @param func The function name where the free is done.
/// @param line The line number where the free is done.
/// @param page The page.
/// @return Returns 0 on success, or -1 if an error occurs.
int pr_free_pages(const char *file, const char *func, int line, page_t *page);

/// Wrapper that provides the filename, the function and line where the alloc is happening.
#define alloc_pages(...) pr_alloc_pages(__RELATIVE_PATH__, __func__, __LINE__, __VA_ARGS__)

/// Wrapper that provides the filename, the function and line where the free is happening.
#define free_pages(...) pr_free_pages(__RELATIVE_PATH__, __func__, __LINE__, __VA_ARGS__)

/// @brief Find the first free 2^order amount of page frames, set it allocated
/// and return the memory address of the first page frame allocated.
/// @param gfp_mask GFP_FLAGS to decide the zone allocation.
/// @param order    The logarithm of the size of the page frame.
/// @return Memory address of the first free page frame allocated.
uint32_t alloc_pages_lowmem(gfp_t gfp_mask, uint32_t order);

/// @brief Frees from the given page frame address up to 2^order amount of page
/// frames.
/// @param vaddr The page frame address.
/// @return Returns 0 on success, or -1 if an error occurs.
int free_pages_lowmem(uint32_t vaddr);

/// @brief Retrieves the total space of the zone corresponding to the given GFP mask.
/// @param gfp_mask The GFP mask specifying the allocation constraints.
/// @return The total space of the zone, or 0 if the zone cannot be retrieved.
unsigned long get_zone_total_space(gfp_t gfp_mask);

/// @brief Retrieves the free space of the zone corresponding to the given GFP mask.
/// @param gfp_mask The GFP mask specifying the allocation constraints.
/// @return The free space of the zone, or 0 if the zone cannot be retrieved.
unsigned long get_zone_free_space(gfp_t gfp_mask);

/// @brief Retrieves the cached space of the zone corresponding to the given GFP mask.
/// @param gfp_mask The GFP mask specifying the allocation constraints.
/// @return The cached space of the zone, or 0 if the zone cannot be retrieved.
unsigned long get_zone_cached_space(gfp_t gfp_mask);

/// @brief Retrieves the buddy system status for the zone associated with the given GFP mask.
/// @param gfp_mask The GFP mask specifying the memory zone (e.g., GFP_KERNEL, GFP_HIGHUSER).
/// @param buffer A pointer to the buffer where the formatted status string will be written.
/// @param bufsize The size of the provided buffer, in bytes. Must be greater than 0.
/// @return The number of characters written to the buffer, or a negative value if an error occurs.
int get_zone_buddy_system_status(gfp_t gfp_mask, char *buffer, size_t bufsize);

/// @brief Checks if the specified address points to a page_t (or field) that
/// belongs to lowmem.
/// @param addr The address to check.
/// @return 1 if it belongs to lowmem, 0 otherwise.
static inline int is_lowmem_page_struct(void *addr)
{
    uint32_t start_lowm_map  = (uint32_t)memory.page_data->node_zones[ZONE_NORMAL].zone_mem_map;
    uint32_t lowmem_map_size = sizeof(page_t) * memory.page_data->node_zones[ZONE_NORMAL].num_pages;
    uint32_t map_index       = (uint32_t)addr - start_lowm_map;
    return map_index < lowmem_map_size;
}
