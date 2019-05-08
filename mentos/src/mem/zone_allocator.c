///                MentOS, The Mentoring Operating system project
/// @file zone_allocator.c
/// @brief Implementation of the Zone Allocator
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "zone_allocator.h"
#include "buddysystem.h"
#include "debug.h"
#include "kheap.h"
#include "kernel.h"
#include "assert.h"
#include "paging.h"

#define MAX_ORDER_ALIGN(addr)                                                  \
	((addr) & (~(PAGE_SIZE * (1 << (MAX_ORDER - 1)) - 1))) +                   \
		(PAGE_SIZE * (1 << (MAX_ORDER - 1)))

/// Defined in kernel.ld, points at the end of kernel's data segment.
extern uint32_t end;

/// End address of the kernel's data segment.
static uint8_t *_mmngr_memory_start = (uint8_t *)(&end);

/// Array of all physical blocks
page_t *mem_map = NULL;

/// Memory node.
pg_data_t *contig_page_data = NULL;

/// @brief       Get the zone that contains a page frame.
/// @param  page A page descriptor.
/// @return      The zone requested.
static zone_t *get_zone_from_page(page_t *page)
{
	zone_t *zone = NULL;
	int nr_zones = contig_page_data->nr_zones;

	for (int zone_index = 0; zone_index < nr_zones; zone_index++) {
		zone = contig_page_data->node_zones + zone_index;
		page_t *last_page = zone->zone_mem_map + zone->size;

		if (page < last_page) {
			return zone;
		}
	}

	// Error: page is over memory size.
	return (zone_t *)NULL;
}

/// @brief           Get a zone from gfp_mask
/// @param  gfp_mask GFP_FLAG see gfp.h.
/// @return          The zone requested.
static zone_t *get_zone_from_flags(gfp_t gfp_mask)
{
	switch (gfp_mask) {
	case GFP_KERNEL:
	case GFP_ATOMIC:
	case GFP_NOFS:
	case GFP_NOIO:
	case GFP_NOWAIT:
		return &contig_page_data->node_zones[ZONE_NORMAL];
	case GFP_HIGHUSER:
		return &contig_page_data->node_zones[ZONE_HIGHMEM];
	default:
		return (zone_t *)NULL;
	}
}

static bool_t is_memory_clean(gfp_t gfp_mask)
{
	bool_t memory_clean = true;
	zone_t *zone = get_zone_from_flags(gfp_mask);
	assert((zone != NULL) && "Invalid zone flag!");

#ifdef ENABLE_BUDDYSYSTEM
	/* Check every field nr_free of the buddy system
     * descriptor of the zone that all blocks are
     * allocated on the last free list.
     */
	unsigned int order = 0;
	for (; order < MAX_ORDER - 1; ++order) {
		free_area_t *area = zone->free_area + order;
		if (area->nr_free != 0) {
			memory_clean = false;

			break;
		}
	}

	if (memory_clean &&
		(zone->free_area[order].nr_free != (zone->size / (1UL << order)))) {
		memory_clean = false;
	}

#else
	/* Check every field _count of the page descriptor
     * of the zone as free.
     */
	for (int i = 0; i < zone->size; ++i) {
		page_t *page = zone->zone_mem_map + i;
		if (page->_count != -1) {
			memory_clean = false;

			break;
		}
	}
#endif

	return memory_clean;
}

/// @brief  Checks if the physical memory manager is working properly.
/// @return If the check was done correctly.
static bool_t pmm_check()
{
	dbg_print(
		"\n=================== ZONE ALLOCATOR TEST ==================== \n");

	dbg_print("\t[STEP1] One page frame in kernel-space... ");
	dbg_print("\n\t ===== [STEP1] One page frame in kernel-space ====\n");
	uint32_t ptr1 = __alloc_page(GFP_KERNEL);
	free_page(ptr1);
	if (!is_memory_clean(GFP_KERNEL)) {
		return false;
	}

	dbg_print("\t[STEP2] Five page frames in user-space... ");
	dbg_print("\n\t ===== [STEP2] Five page frames in user-space ====\n");
	uint32_t ptr2[5];
	for (int i = 0; i < 5; i++) {
		ptr2[i] = __alloc_page(GFP_HIGHUSER);
	}
	for (int i = 0; i < 5; i++) {
		free_page(ptr2[i]);
	}
	if (!is_memory_clean(GFP_HIGHUSER)) {
		return false;
	}

	dbg_print("\t[STEP3] 2^{3} page frames in kernel-space... ");
	dbg_print("\n\t ===== [STEP3] 2^{3} page frames in kernel-space ====\n");
	uint32_t ptr3 = __alloc_pages(GFP_KERNEL, 3);
	free_pages(ptr3, 3);
	if (!is_memory_clean(GFP_KERNEL)) {
		return false;
	}

	dbg_print("\t[STEP4] Five 2^{i} page frames in user-space... ");
	dbg_print("\n\t ===== [STEP4] Five 2^{i} page frames in user-space ====\n");
	uint32_t ptr4[5];
	for (int i = 0; i < 5; i++) {
		ptr4[i] = __alloc_pages(GFP_HIGHUSER, i);
	}
	for (int i = 0; i < 5; i++) {
		free_pages(ptr4[i], i);
	}
	if (!is_memory_clean(GFP_HIGHUSER)) {
		return false;
	}

	dbg_print("\t[STEP5] Mixed page frames in kernel-space... ");
	dbg_print("\n\t ===== [STEP5] Mixed page frames in kernel-space ====\n");
	int **ptr = (int **)__alloc_page(GFP_KERNEL);
	int i = 0;
	for (; i < 5; ++i) {
		ptr[i] = (int *)__alloc_page(GFP_KERNEL);
	}
	for (; i < 20; ++i) {
		ptr[i] = (int *)__alloc_pages(GFP_KERNEL, 2);
	}

	int j = 0;
	for (; j < 5; ++j) {
		free_page((uint32_t)ptr[j]);
	}
	for (; j < 20; ++j) {
		free_pages((uint32_t)ptr[j], 2);
	}
	free_page((uint32_t)ptr1);

	if (!is_memory_clean(GFP_KERNEL)) {
		return false;
	}
	return true;
}

/// @brief      Initialize Buddy System.
/// @param zone A memory zone.
static void buddy_system_init(zone_t *zone)
{
    // Initialize the free_lists of each area of the zone.
    for (unsigned int order = 0; order < MAX_ORDER; order++) {
        free_area_t *area = zone->free_area + order;
        area->nr_free = 0;
        list_head_init(&area->free_list);
    }

    // Current base page descriptor of the zone.
    page_t *page = zone->zone_mem_map;
    // Address of the last page descriptor of the zone.
    page_t *last_page = page + zone->size;

    // Get the free area collecting the larges block of page frames.
    const unsigned int order = MAX_ORDER - 1;
    free_area_t *area = zone->free_area + order;

    // Add all zone's pages to the largest free area block.
    uint32_t block_size = 1UL << order;
    while ((page + block_size) <= last_page) {
        /* page has already the _count field set to -1,
         * therefore only save the order of the page.
         */
        page->private = order;

        // Insert page as first element in the list.
        list_head_add_tail(&page->lru, &area->free_list);
        // Increase the number of free block of the free_area_t.
        area->nr_free++;

        page += block_size;
    }

    assert(page == last_page &&
           "Memory size is not aligned to MAX_ORDER size!");
}

/// @brief Initializes the memory attributes.
/// @param name       Zone's name.
/// @param zone_index Zone's index.
/// @param adr_from   the lowest address of the zone
/// @param adr_to     the highest address of the zone (not included!)
static void zone_init(char *name, int zone_index, uint32_t adr_from,
					  uint32_t adr_to)
{
	assert((adr_from < adr_to) && ((adr_from & 0xfffff000) == adr_from) &&
		   ((adr_to & 0xfffff000) == adr_to) &&
		   "Inserted bad block addresses!");

	// Take the zone_t structure that correspondes to the zone_index.
	zone_t *zone = contig_page_data->node_zones + zone_index;

	// Number of page frames in the zone.
	size_t num_page_frames = (adr_to - adr_from) / PAGE_SIZE;

	// Index of the first page frame of the zone.
	uint32_t first_page_frame = adr_from / PAGE_SIZE;

	// Update zone info.
	zone->name = name;
	zone->size = num_page_frames;
	zone->free_pages = num_page_frames;
	zone->zone_mem_map = mem_map + first_page_frame;
	zone->zone_start_pfn = first_page_frame;

	dbg_print("ZONE %s, first page: %p, last page: %p, npages:%d\n", zone->name,
			  zone->zone_mem_map, zone->zone_mem_map + zone->size, zone->size);

#ifdef ENABLE_BUDDYSYSTEM
	buddy_system_init(zone);
	buddy_system_dump(zone);
#endif
}

unsigned int find_nearest_order_greater(uint32_t amount)
{
	amount = (amount + PAGE_SIZE - 1) / PAGE_SIZE;
	unsigned int order = 0;
	while ((1UL << order) < amount) {
		++order;
	}

	return order;
}

bool_t pmmngr_init(uint32_t mem_size)
{
	//==== Get RAM size =====================================================
	// Align mem_size to a page frame size, namely 4KB.
	uint32_t _mmngr_memory_size = MAX_ORDER_ALIGN(mem_size);

	// Total number of blocks (all RAM).
	uint32_t _mmngr_num_frames = _mmngr_memory_size / PAGE_SIZE;
	//=======================================================================

	//==== Skip modules =====================================================
	dbg_print("[PMM] Start memory address previous skip modules : 0x%p \n",
			  _mmngr_memory_start);

	for (int i = 0; i < MAX_MODULES; i++) {
		uint8_t *skip_addr = (uint8_t *)module_end[i];
		if (skip_addr != NULL && _mmngr_memory_start < skip_addr) {
			_mmngr_memory_start = skip_addr;
		}
	}

	dbg_print("[PMM] Start memory address after skip modules    : 0x%p \n",
			  _mmngr_memory_start);
	//=======================================================================

	//==== Initialize array of page_t =======================================
	dbg_print("[PMM] Initializing memory map structure...\n");
	mem_map = (page_t *)_mmngr_memory_start;

	// Initialize each page_t.
	for (int page_index = 0; page_index < _mmngr_num_frames; ++page_index) {
		page_t *page = mem_map + page_index;
		// Mark page as free.
		page->_count = -1;
		list_head_init(&(page->lru));
	}
	//=======================================================================

	//==== Skip memory space used for page_t[] ==============================
	_mmngr_memory_start += sizeof(page_t) * _mmngr_num_frames;
	dbg_print(
		"[PMM] Size of mem_map                            : %i byte [0x%p - 0x%p]\n",
		(void *)_mmngr_memory_start - (void *)mem_map, mem_map,
		_mmngr_memory_start);
	//=======================================================================

	//==== Initialize contig_page_data node =================================
	dbg_print("[PMM] Initializing contig_page_data node...\n");
	contig_page_data = (pg_data_t *)_mmngr_memory_start;
	// ZONE_NORMAL and ZONE_HIGHMEM
	contig_page_data->nr_zones = __MAX_NR_ZONES;
	// NID start from 0.
	contig_page_data->node_id = 0;
	// Corresponds with mem_map.
	contig_page_data->node_mem_map = mem_map;
	// In UMA we have only one node.
	contig_page_data->node_next = NULL;
	// All the memory.
	contig_page_data->node_size = _mmngr_num_frames;
	// mem_map[0].
	contig_page_data->node_start_mapnr = 0;
	// The first physical page.
	contig_page_data->node_start_paddr = 0x0;
	//=======================================================================

	//==== Skip memory space used for pg_data_t =============================
	_mmngr_memory_start += sizeof(pg_data_t);
	//=======================================================================

	//==== Initialize zones zone_t ==========================================
	dbg_print("[PMM] Initializing zones...\n");

	// ZONE_NORMAL   [ memory_start - mem_size/4 ]
	uint32_t start_normal_addr = MAX_ORDER_ALIGN((uint32_t)_mmngr_memory_start);
	uint32_t stop_normal_addr = MAX_ORDER_ALIGN(_mmngr_memory_size >> 2);
	zone_init("Normal", ZONE_NORMAL, start_normal_addr, stop_normal_addr);

	// ZONE_HIGHMEM  [ mem_size/4 - mem_size ]
	uint32_t start_high_addr = stop_normal_addr;
	uint32_t stop_high_addr = _mmngr_memory_size;
	zone_init("HighMem", ZONE_HIGHMEM, start_high_addr, stop_high_addr);
	//=======================================================================

	dbg_print("[PMM] Memory Size                                : %u MB \n",
			  _mmngr_memory_size / M);
	dbg_print("[PMM] Total page frames    (MemorySize/4096)     : %u \n",
			  _mmngr_num_frames);
	dbg_print("[PMM] mem_map address                            : 0x%p \n",
			  mem_map);
	dbg_print("[PMM] Memory Start                               : 0x%p \n",
			  _mmngr_memory_start);

	return pmm_check();
}

uint32_t __alloc_page(gfp_t gfp_mask)
{
	return __alloc_pages(gfp_mask, 0);
}

void free_page(uint32_t addr)
{
	free_pages(addr, 0);
}

uint32_t __alloc_pages(gfp_t gfp_mask, uint32_t order)
{
	zone_t *zone = get_zone_from_flags(gfp_mask);
	assert((zone != NULL) && "Invalid zone flag!");
	assert((order <= (MAX_ORDER - 1)) && "Invalid order!");

	int block_size = 1UL << order;

	page_t *page = NULL;
#ifdef ENABLE_BUDDYSYSTEM
	// Search for a block of page frames by using the BuddySystem.
	page = bb_alloc_pages(zone, order);

#else
	// First page of the zone.
	page_t *block = zone->zone_mem_map;
	// Last page of the zone.
	page_t *last_frame = zone->zone_mem_map + zone->size - order + 1;
	// When true, then we found a block of pages
	int found = 0;
	// Search for a block of pages
	while (block < last_frame && found == 0) {
		// Suppose this is the right block.
		found = 1;
		// Check if enough pages are available in current block.
		for (unsigned int i = 0; i < block_size; ++i) {
			found = ((block + i)->_count == -1);
			if (!found) {
				/* The block is not large enough. We have to skip it
                     * and restart to search for another one.
                     */
				block += i + 1;

				break;
			}
		}

		if (found) {
			// Set the found block of pages as taken, and not available.
			for (unsigned int i = 0; i < block_size; ++i) {
				(block + i)->_count = 0;
			}
		}
	}
	page = (found == 1) ? block : NULL;
#endif

	uint32_t block_frame_adr = -1;
	if (page != NULL) {
		// Decrement the number of pages in the zone.
		zone->free_pages -= block_size;
		// Get the index of the first page frame of the block.
		uint32_t delta_block_frame = (uint32_t)(page - zone->zone_mem_map);
		block_frame_adr = zone->zone_start_pfn + delta_block_frame;
		block_frame_adr = block_frame_adr * PAGE_SIZE;
	}

	if (block_frame_adr == -1) {
		dbg_print("MEM. REQUEST FAILED");
	} else {
		dbg_print("BS-G: addr: %p (page: %p order: %d)\n", block_frame_adr,
				  page, order);
	}

	return block_frame_adr;
}

void free_pages(uint32_t addr, uint32_t order)
{
	page_t *page = mem_map + (addr / PAGE_SIZE);
	zone_t *zone = get_zone_from_page(page);
	assert((zone != NULL) && "Page is over memory size");

	int block_size = 1UL << order;

#ifdef ENABLE_BUDDYSYSTEM
	bb_free_pages(zone, page, order);
#else
	// Set the given block of page frames as free, and available again.
	for (unsigned int i = 0; i < block_size; ++i) {
		(page + i)->_count = -1;
	}
#endif

	zone->free_pages += block_size;

	dbg_print("BS-F: addr: %p (page: %p order: %d)\n", addr, page, order);
}

uint32_t get_memory_start()
{
	return (uint32_t)_mmngr_memory_start;
}
