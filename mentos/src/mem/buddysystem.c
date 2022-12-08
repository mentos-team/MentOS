/// @file buddysystem.c
/// @brief Buddy System.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[BUDDY ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_DEBUG

#include "mem/buddysystem.h"
#include "mem/paging.h"
#include "assert.h"
#include "io/debug.h"
#include "system/panic.h"

/// @brief Cache level low limit after which allocation starts.
#define LOW_WATERMARK_LEVEL 10
/// @brief Cache level high limit, above it deallocation happens.
#define HIGH_WATERMARK_LEVEL 70
/// @brief Cache level midway limit.
#define MID_WATERMARK_LEVEL ((LOW_WATERMARK_LEVEL + HIGH_WATERMARK_LEVEL) / 2)

/// @brief Bitwise flags for identifying page types and statuses.
enum bb_flag {
    FREE_PAGE = 0, ///< Bit position that identifies when a page is free or not.
    ROOT_PAGE = 1  ///< Bit position that identifies when a page is the root page.
};

/// @brief Sets the given flag in the page.
/// @param page The page of which we want to modify the flag.
/// @param flag The flag we want to set.
static inline void __bb_set_flag(bb_page_t *page, int flag)
{
    set_bit(flag, &page->flags);
}

/// @brief Clears the given flag from the page.
/// @param page The page of which we want to modify the flag.
/// @param flag The flag we want to clear.
static inline void __bb_clear_flag(bb_page_t *page, int flag)
{
    clear_bit(flag, &page->flags);
}

/// @brief Gets the given flag from the page.
/// @param page The page of which we want to modify the flag.
/// @param flag The flag we want to test.
/// @return 1 if the bit is set, 0 otherwise.
static inline int __bb_test_flag(bb_page_t *page, int flag)
{
    return test_bit(flag, &page->flags);
}

/// @brief Returns the page at the given index, starting from the given base.
/// @param instance The buddy system instance we are working with.
/// @param base     The base page from which we move at the given index.
/// @param index    The number of pages we want to move from the base.
/// @return The page we found.
static inline bb_page_t *__get_page_from_base(bb_instance_t *instance, bb_page_t *base, unsigned int index)
{
    return (bb_page_t *)(((uint32_t)base) + instance->pgs_size * index);
}

/// @brief Returns the page at the given index, starting from the first page of the BB system.
/// @param instance The buddy system instance we are working with.
/// @param index    The number of pages we want to move from the first page.
/// @return The page we found.
static inline bb_page_t *__get_page_at_index(bb_instance_t *instance, unsigned int index)
{
    return __get_page_from_base(instance, instance->base_page, index);
}

/// @brief Computes the number of pages separating the two pages (begin, end).
/// @param instance the buddy system instance we are working with.
/// @param begin    the first page.
/// @param end      the second page.
/// @return The number of pages between begin and end.
static inline unsigned int __get_page_range(bb_instance_t *instance, bb_page_t *begin, bb_page_t *end)
{
    return (((uintptr_t)end) - ((uintptr_t)begin)) / instance->pgs_size;
}

/// @brief Get the buddy index of a page.
/// @details
///  ----------------------- xor -----------------------
/// | page_idx    ^   (1UL << order)    =     buddy_idx |
/// |     1                  1                    0     |
/// |     0                  1                    1     |
///  ---------------------------------------------------
/// If the bit of page_idx that corresponds to the block
/// size, is 1, then we have to take the block on the
/// left (0), otherwise we have to take the block on the right (1).
/// @param  page_idx the page index.
/// @param  order    the logarithm of the size of the block.
/// @return the page index of the buddy of page.
static inline unsigned long __get_buddy_at_index(unsigned long page_idx, unsigned int order)
{
    return (page_idx ^ (1UL << order));
}

/// @brief Returns the pointer to the free-area manager for the given order.
/// 
/// @param instance the buddysystem instance.
/// @param order    the desired order.
/// @return pointer to the free-area manager.
static inline bb_free_area_t *__get_area_of_order(bb_instance_t *instance, unsigned int order)
{
    return instance->free_area + order;
}

/// @brief Checks if the page is FREE and has the same order.
/// @param page  the page to check.
/// @param order the oder to check.
/// @return true if the page is buddy, false otherwise.
static inline bool_t __page_is_buddy(bb_page_t *page, unsigned int order)
{
    return __bb_test_flag(page, FREE_PAGE) && (page->order == order);
}

bb_page_t *bb_alloc_pages(bb_instance_t *instance, unsigned int order)
{
    bb_page_t *page      = NULL;
    bb_free_area_t *area = NULL;

    // Cyclic search through each list for an available block,
    // starting with the list for the requested order and
    // continuing if necessary to larger orders.
    unsigned int current_order;
    for (current_order = order; current_order < MAX_BUDDYSYSTEM_GFP_ORDER; ++current_order) {
        // Get the free_area_t at index 'current_order'.
        area = /* ... */;
        // Check if area is not empty, which means that there is at least a block inside the list.
        if (!list_head_empty(&/* ... */)) {
            goto block_found;
        }
    }

    // No suitable free block has been found.
    return NULL;

block_found:
    // Get a block of pages from the found free_area_t. Here we have to manage
    // pages. Recall, free_area_t collects the first page_t of each free block
    // of 2^order contiguous page frames.
    page = list_entry(/* ... */, /* ... */, /* ... */);

    // Remove the descriptor of its first page frame.
    list_head_remove(&/* ... */);

    // Set the page as allocated, thus, remove the flag FREE_PAGE.
    __bb_clear_flag(/* ... */, /* ... */);

    // Check that the page is a ROOT_PAGE
    assert(__bb_test_flag(/* ... */, /* ... */));

    // Decrease the number of free block of the free_area_t.
    /* ... */ -= 1;

    // Found a block of 2^k page frames, to satisfy a request
    // for 2^h page frames (h < k) the program allocates
    // the first 2^h page frames and iteratively reassigns
    // the last 2^k – 2^h page frames to the free_area lists
    // that have indexes between h and k.
    unsigned int size = 1UL << current_order;
    while (current_order > order) {
        // At each loop, we have to set free the right half of the found block.

        // Refer to the lower free_area_t and order.
        area--;
        current_order--;

        // Split the block and set free the second half.
        size >>= /* ... */;

        // Get the address of the page in the midle of the found block.
        bb_page_t *buddy = __get_page_from_base(/* ... */, /* ... */, /* ... */);

        // Check that the buddy is free and not a root page.
        assert(/* ... */ &&!/* ... */);

        // Insert buddy as first element in the list of available blocks (free_list).
        list_head_insert_after(&/* ... */.siblings, &/* ... */);

        // Increase the number of free block of the free_area_t.
        /* ... */ += 1;

        // Save the order of the buddy.
        /* ... */ = current_order;

        // Set the buddy as a root page.
        /* ... */;
    }

    // Set the order of the page we are returning.
    /* ... */;

    // Set the page as root page.
    /* ... */;

    // Clear the free-page status from the page.
    /* ... */;

#if 0
    buddy_system_dump(instance);
#endif
    return page;
}

void bb_free_pages(bb_instance_t *instance, bb_page_t *page)
{
    // Take the first page descriptor of the zone.
    bb_page_t *base = instance->base_page;
    // Take the page frame index of page compared to the zone.
    unsigned long page_idx = __get_page_range(instance, base, page);
    // Set the page freed, but do not set the private
    // field because we want to try to merge. FIXME: Add this line on zone page deallocate!
    //	set_page_count(page, -1);

    unsigned int order = page->order;

    // Check that the page is free, or that it is not a root page.
    if (/* ... */ || !/* ... */) {
        kernel_panic("Double deallocation in buddy system!");
    }

    // Performs a cycle that starts with the smallest
    // order block and moves up to the top order.
    while (order < MAX_BUDDYSYSTEM_GFP_ORDER - 1) {
        // Get the base page index.
        page = __get_page_from_base(instance, base, page_idx);

        // Get the index of the buddy.
        unsigned long buddy_idx = __get_buddy_at_index(page_idx, order);

        // Return the page descriptor of the buddy.
        bb_page_t *buddy = __get_page_from_base(instance, base, buddy_idx);

        // If the page is not a buddy, stop the loop. So it should not be free
        // and having the same size.
        if (!(/* ... */ &&/* ... */))
            break;

        // If buddy is free, remove buddy from the current free list,
        // because then the coalesced block will be inserted on a
        // upper order.
        /* ... */;

        // Decrease the number of free block of the current free_area_t.
        /* ... */;

        // Get the page that gets forgotten, it's always the one on the right (the greatest)
        bb_page_t *forgot_page = buddy > page ? buddy : page;

        // Clear the root flag from the forgotten page.
        /* ... */;

        // Set the forgotten page as a free page.
        /* ... */;

        // Update the page index with the index of the coalesced block.
        /* ... */ &= /* ... */;

        order++;
    }

    // Take the coalesced block with the order reached up.
    bb_page_t *coalesced = __get_page_from_base(instance, base, page_idx);

    // Update the order of the coalesced page.
    /* ... */;

    // Set it to be a root page and a free page
    /* ... */;
    /* ... */;

    // Insert coalesced as first element in the free list.
    list_head_insert_after(&/* ... */, &/* ... */);

    // Increase the number of free block of the free_area_t.
    /* ... */;

#if 0
    buddy_system_dump(instance);
#endif
}

void buddy_system_init(bb_instance_t *instance,
                       const char *name,
                       void *pages_start,
                       uint32_t bbpage_offset,
                       uint32_t pages_stride,
                       uint32_t pages_count)
{
    // Compute the base base page of the buddysystem instance.
    instance->base_page = ((bb_page_t *)(((uint32_t)pages_start) + bbpage_offset));
    // Save all needed page info.
    instance->bbpg_offset = bbpage_offset;
    instance->pgs_size    = pages_stride;
    instance->size        = pages_count;
    instance->name        = name;

    // Initialize all pages.
    for (uint32_t index = 0; index < pages_count; ++index) {
        // Get the page at the given index.
        bb_page_t *page = __get_page_at_index(instance, index);
        // Initialize the flags of the page.
        page->flags = 0;
        // Mark page as free.
        __bb_set_flag(page, FREE_PAGE);
        // Initialize siblings list.
        list_head_init(&(page->location.siblings));
        // N.B.: The order is initialized afterwards.
    }

    // Initialize the free_lists of each area of the zone.
    for (unsigned int order = 0; order < MAX_BUDDYSYSTEM_GFP_ORDER; order++) {
        // Get the area that manages the given order.
        bb_free_area_t *area = __get_area_of_order(instance, order);
        // Initialize the number of free pages.
        area->nr_free = 0;
        // Initialize linked list of free pages.
        list_head_init(&area->free_list);
    }

    // Current base page descriptor of the zone.
    bb_page_t *page = instance->base_page;
    // Address of the last page descriptor of the zone.
    bb_page_t *last_page = __get_page_from_base(instance, page, instance->size);
    // Initially, all the memory is divided into blocks of the higher order.
    const unsigned int max_order = MAX_BUDDYSYSTEM_GFP_ORDER - 1;
    // Get the free area collecting the larges block of page frames.
    bb_free_area_t *area = __get_area_of_order(instance, max_order);
    // Compute the block size.
    uint32_t block_size = 1UL << max_order;
    // Add all zone's pages to the largest free area block.
    while ((page + block_size) <= last_page) {
        // Save the order of the page.
        page->order = max_order;
        // Set the page as root.
        __bb_set_flag(page, ROOT_PAGE);
        // Insert the page inside the list of free pages of the area.
        list_head_insert_before(&page->location.siblings, &area->free_list);
        // Increase the number of free block of the area.
        area->nr_free++;
        // Move to the next page.
        page = __get_page_from_base(instance, page, block_size);
    }
    // Check that the page we have reached with the iteration is the last page.
    assert(page == last_page && "Memory size is not aligned to MAX_ORDER size!");
}

void buddy_system_dump(bb_instance_t *instance)
{
    // Print free_list's size of each area of the zone.
    pr_debug("Zone %-12s ", instance->name);
    for (int order = 0; order < MAX_BUDDYSYSTEM_GFP_ORDER; order++) {
        bb_free_area_t *area = instance->free_area + order;
        pr_debug("%2d ", area->nr_free);
    }
    pr_debug(": %s\n", to_human_size(buddy_system_get_free_space(instance)));
}

unsigned long buddy_system_get_total_space(bb_instance_t *instance)
{
    return instance->size * PAGE_SIZE;
}

unsigned long buddy_system_get_free_space(bb_instance_t *instance)
{
    unsigned int size = 0;
    for (int order = 0; order < MAX_BUDDYSYSTEM_GFP_ORDER; ++order)
        size += instance->free_area[order].nr_free * (1UL << order) * PAGE_SIZE;
    return size;
}

unsigned long buddy_system_get_cached_space(bb_instance_t *instance)
{
    unsigned int size = 0;
    for (int order = 0; order < MAX_BUDDYSYSTEM_GFP_ORDER; ++order)
        size += instance->free_pages_cache_size * PAGE_SIZE;
    return size;
}

static void __cache_extend(bb_instance_t *instance, int count)
{
    for (int i = 0; i < count; i++) {
        bb_page_t *page = bb_alloc_pages(instance, 0);
        list_head_insert_after(&page->location.cache, &instance->free_pages_cache_list);
        instance->free_pages_cache_size++;
    }
}

static void __cache_shrink(bb_instance_t *instance, int count)
{
    for (int i = 0; i < count; i++) {
        list_head *page_list = list_head_pop(&instance->free_pages_cache_list);
        bb_page_t *page      = list_entry(page_list, bb_page_t, location.cache);
        bb_free_pages(instance, page);
        instance->free_pages_cache_size--;
    }
}

static bb_page_t *__cached_alloc(bb_instance_t *instance)
{
    if (instance->free_pages_cache_size < LOW_WATERMARK_LEVEL) {
        // Request pages from the buddy system
        uint32_t pages_to_request = MID_WATERMARK_LEVEL - instance->free_pages_cache_size;
        __cache_extend(instance, pages_to_request);
    }
    list_head *page_list = list_head_pop(&instance->free_pages_cache_list);
    bb_page_t *page      = list_entry(page_list, bb_page_t, location.cache);
    return page;
}

static void __cached_free(bb_instance_t *instance, bb_page_t *page)
{
    list_head_insert_after(&page->location.cache, &instance->free_pages_cache_list);

    if (instance->free_pages_cache_size > HIGH_WATERMARK_LEVEL) {
        // Free pages to the buddy system
        uint32_t pages_to_free = instance->free_pages_cache_size - MID_WATERMARK_LEVEL;
        __cache_shrink(instance, pages_to_free);
    }
}

bb_page_t *bb_alloc_page_cached(bb_instance_t *instance)
{
    return __cached_alloc(instance);
}

void bb_free_page_cached(bb_instance_t *instance, bb_page_t *page)
{
    __cached_free(instance, page);
}
