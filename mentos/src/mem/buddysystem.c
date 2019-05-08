/// @file buddysystem.c
/// @brief Buddy System.
/// @date Apr 2019.

#include "buddysystem.h"
#include "debug.h"
#include "assert.h"

/// @brief           Get the buddy index of a page.
/// @param  page_idx A page index.
/// @param  order    The logarithm of the size of the block.
/// @return          The page index of the buddy of page.
static unsigned long get_buddy_idx(unsigned long page_idx, unsigned int order)
{
	/*  Get the index of the buddy block.
     *
     *  ----------------------- xor -----------------------
     * | page_idx    ^   (1UL << order)    =     buddy_idx |
     * |     1                  1                    0     |
     * |     0                  1                    1     |
     *  ---------------------------------------------------
     *
     * If the bit of page_idx that corresponds to the block
     * size, is 1, then we have to take the block on the
     * left (0), otherwise we have to take the block on the right (1).
     */
	unsigned long buddy_idx = page_idx ^ (1UL << order);

	return buddy_idx;
}


page_t *bb_alloc_pages(zone_t *zone, unsigned int order)
{
	page_t *page = NULL;
	free_area_t *area = NULL;

	// Search for a free_area_t with at least one available block of pages.
	unsigned int current_order;
	for (current_order = order; current_order < MAX_ORDER; ++current_order) {
		// get the free_area_t at index 'current_order'
		area = // ...

		// check if area is not empty (is there at least a block here?)
		if (!list_head_empty(&/*...*/)) {
			goto block_found;
		}
	}

	// No suitable free block has been found.
	return NULL;

block_found:
	// Get a block of pages from the found free_area_t.
	// Here we have to manage pages. Recall, free_area_t collects the first
	// page_t of each free block of 2^order contiguous page frames.

	page = list_entry(/*...*/);

	// Remove page from the list_head in the found free_area_t.
	list_head_del(/*...*/);

	// Set page as taken.
	page->_count = // ...
	page->private = 0;

	// Decrease the number of free blocks in the found free_area_t.
	// ...

	/* We found a block with 2^k page frames to satisfy a request
     * of 2^h page frames. If h < k, then we can split the block with 2^k
	 * pages until it is large 2^h pages, namely k == h.
     */

	// We can exploit size(=2^k) to have at each loop the address the page that
	// resides in the midle of the found block.
	unsigned int size = 1 << current_order;
	while (current_order > order) {

		// At each loop, we have to set free the right half of the found block.

		// Split the block size in half
		size = // ...

		// get the address of the page in the midle of the found block.
		page_t *buddy = // ...

		// set the order of pages after the buddy page_t (the field 'private')
		// ...

		// get the free_area_t collecting blocks with 2^(k-1) page frames
		area = // ...

		// add the buddy block in its list of available blocks
		// ...

		// Increase the number of free blocks of the free_area_t.
		// ...
	}

	buddy_system_dump(zone);

	return page;
}

void bb_free_pages(zone_t *zone, page_t *page, unsigned int order)
{
	// Take the first page descriptor of the zone.
	page_t *base = zone->zone_mem_map;

	// Take the page frame index of page compared to the zone.
	unsigned long page_idx = page - base;

	// Set the given page as free
	page->_count = // ...

	// At each loop, check if the buddy block can be merged with page.
	while (order < MAX_ORDER - 1) {
		// Get the index of the buddy block of page.
		unsigned long buddy_idx = get_buddy_idx(page_idx, order);
		// Get the page_t of the buddy block, namely its first page frame.
		page_t *buddy = base + buddy_idx;

		// If the buddy is free and it has the same size of page, then
		// they can be merged. Otherwise, we can stop the while-loop and insert
		// page in the list of free blocks.

		if (!(/*...it is free...*/ && /*...they have the same size...*/)) {
			break;
		}

		// we are here only if buddy is free and can be merged with page.

		// remove buddy from the list of available blocks in its free_area_t
		// ....

		// Decrease the number of free block of the current free_area_t.
		// ...

		// buddy no longer represents a free block, so clear the private field.
		buddy->private = 0;

		// Update the page index with the index of the coalesced block.
		// ...

		order++;
	}

	// The coalesced block is the result of the merging procedure.
	page_t *coalesced = base + page_idx;

	// Update the field private to set the size.
	coalesced->private = // ...

	// Insert the coalesced block in the free_area as available block
	// ...

	// Increase the number of free blocks of the free_area.
	// ...

	buddy_system_dump(zone);
}

void buddy_system_dump(zone_t *zone)
{
	// Print free_list's size of each area of the zone.
	dbg_print("Zone\t%s\t", zone->name);
	for (int order = 0; order < MAX_ORDER; order++) {
		free_area_t *area = zone->free_area + order;
		dbg_print("%d\t", area->nr_free);
	}
	dbg_print("\n");
}
