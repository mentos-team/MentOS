///                MentOS, The Mentoring Operating system project
/// @file zone_allocator.h
/// @brief Implementation of the Zone Allocator
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "gfp.h"
#include "math.h"
#include "stdint.h"
#include "stdbool.h"
#include "list_head.h"

/// Max order of buddysystem blocks.
#define MAX_ORDER 11

/// @brief Buddy system descriptor: collection of free page blocks.
/// Each block represents 2^k free contiguous page.
typedef struct {
	/// free_list collectes the first page descriptors of a blocks of 2^k frames
	list_head free_list;
	/// nr_free specifies the number of blocks of free pages.
	int nr_free;
} free_area_t;

/// @brief Page descriptor. Use as a bitmap to understand
/// the order of the block and if it is free or allocated.
typedef struct {
	/// Array of flags encoding also the zone number to which the page frame belongs.
	unsigned long flags;
	/// Page frame’s reference counter. -1 free, >= 0 used
	int _count;
	/// If the page is free, this field is used by the buddy system.
	unsigned long private;
	/// Contains pointers to the least recently used doubly linked list of pages.
	struct list_head lru;
} page_t;

/// @brief Enumeration for zone_t.
enum zone_type {
	/*
     * ZONE_DMA is used when there are devices that are not able
     * to do DMA to all of addressable memory (ZONE_NORMAL). Then we
     * carve out the portion of memory that is needed for these devices.
     * The range is arch specific.
     *
     * Some examples
     *
     * Architecture		Limit
     * ---------------------------
     * parisc, ia64, sparc	<4G
     * s390			<2G
     * arm			Various
     * alpha		Unlimited or 0-16MB.
     *
     * i386, x86_64 and multiple other arches
     * 			<16M.
     */

	// ///< Direct memory access.
	// ZONE_DMA,

	/*
     * Normal addressable memory is in ZONE_NORMAL. DMA operations can be
     * performed on pages in ZONE_NORMAL if the DMA devices support
     * transfers to all addressable memory.
     */
	/// Direct mapping. Used by the kernel.
	ZONE_NORMAL,

	/*
     * A memory area that is only addressable by the kernel through
     * mapping portions into its own address space. This is for example
     * used by i386 to allow the kernel to address the memory beyond
     * 900MB. The kernel will set up special mappings (page
     * table entries on i386) for each page that the kernel needs to
     * access.
     */
	/// Page tables mapping. Used by user processes.
	ZONE_HIGHMEM,

	__MAX_NR_ZONES
};

/// @brief Data structure to differentiate memory zone.
typedef struct {
	/// Number of free pages in the zone.
	unsigned long free_pages;
	/// BuddySystem structure for the zone.
	free_area_t free_area[MAX_ORDER];
	/// Pointer to first page descriptor of the zone.
	page_t *zone_mem_map;
	/// Index of the first page frame of the zone.
	uint32_t zone_start_pfn;
	/// Zone's name.
	char *name;
	/// Zone's size in number of pages.
	unsigned long size;
} zone_t;

/// @brief Data structure to rapresent a memory node.
///        In UMA Architecture there is only one node called
///        contig_page_data.
typedef struct {
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
	/// Pointer to the next node.
	struct pglist_data *node_next;
} pg_data_t;

extern page_t *mem_map;
extern pg_data_t *contig_page_data;

/// @brief  	   Find the nearest block's order of size greater
/// 			   than the amount of byte.
/// @param  amount The amount of byte which we want to calculate order.
/// @return 	   The block's order greater and nearest than amount.
uint32_t find_nearest_order_greater(uint32_t amount);

/// @brief 			Physical memory manager initialization (page_t, zones)
/// @param mem_size Size of the memory.
bool_t pmmngr_init(uint32_t mem_size);

/// @brief  		 Find the first free page frame, set it allocated
/// 				 and return the memory address of the page frame.
/// @param  gfp_mask GFP_FLAGS to decide the zone allocation.
/// @return 		 Memory address of the first free block.
uint32_t __alloc_page(gfp_t gfp_mask);

/// @brief 		Frees the given page frame address.
/// @param addr The block address.
void free_page(uint32_t addr);

/// @brief 			Find the first free 2^order amount of page frames,
/// 				set it allocated and return the memory address of the first
/// 				page frame allocated.
/// @param gfp_mask GFP_FLAGS to decide the zone allocation.
/// @param order    The logarithm of the size of the page frame.
/// @return 		Memory address of the first free page frame allocated.
uint32_t __alloc_pages(gfp_t gfp_mask, uint32_t order);

/// @brief 		 Frees from the given page frame address up to
/// 			 2^order amount of page frames.
/// @param addr  The page frame address.
/// @param order The logarithm of the size of the block.
void free_pages(uint32_t addr, uint32_t order);

/// @brief Get the pointer of the last byte allocated with pmmngr_init()
/// @return The pointer to the memory.
uint32_t get_memory_start();
