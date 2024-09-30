/// @file slab.c
/// @brief Memory slab allocator implementation in kernel. This file provides
/// functions for managing memory allocation using the slab allocator technique.
/// Slab allocators are efficient in managing frequent small memory allocations
/// with minimal fragmentation.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[SLAB  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "assert.h"
#include "mem/paging.h"
#include "mem/slab.h"
#include "mem/zone_allocator.h"

/// @brief Structure to represent an individual memory object within a slab.
/// @details This structure is used to manage individual objects allocated from
/// the slab. It contains a linked list to connect objects in the cache.
typedef struct kmem_obj {
    /// @brief Linked list node for tracking objects in the slab.
    list_head objlist;
} kmem_obj_t;

/// @brief Maximum order of kmalloc cache allocations.
/// @details If a requested memory allocation exceeds this order, a raw page
/// allocation is done instead of using the slab cache.
#define MAX_KMALLOC_CACHE_ORDER 12

/// @brief Overhead size for each memory object in the slab cache.
/// @details This defines the extra space required for managing the object,
/// including the `kmem_obj` structure itself.
#define KMEM_OBJ_OVERHEAD sizeof(kmem_obj_t)

/// @brief Initial object count for each slab.
/// @details The starting number of objects in a newly allocated slab cache.
#define KMEM_START_OBJ_COUNT 8

/// @brief Maximum number of objects to refill in a slab cache at once.
/// @details This defines the upper limit on how many objects to replenish in
/// the slab when it runs out of free objects.
#define KMEM_MAX_REFILL_OBJ_COUNT 64

/// @brief Macro to convert an address into a kmem_obj pointer.
/// @param addr Address of the object.
/// @return Pointer to a kmem_obj structure.
#define KMEM_OBJ_FROM_ADDR(addr) ((kmem_obj_t *)(addr))

/// @brief Macro to get the address from a kmem_obj structure.
/// @param object Pointer to the kmem_obj structure.
/// @return Address of the object as a `void *`.
#define ADDR_FROM_KMEM_OBJ(object) ((void *)(object))

/// @brief List of all active memory caches in the system.
static list_head kmem_caches_list;

/// @brief Cache used for managing metadata about the memory caches themselves.
static kmem_cache_t kmem_cache;

/// @brief Array of slab caches for different orders of kmalloc.
static kmem_cache_t *malloc_blocks[MAX_KMALLOC_CACHE_ORDER];

/// @brief Allocates and initializes a new slab page for a memory cache.
/// @param cachep Pointer to the memory cache (`kmem_cache_t`) for which a new
/// slab page is being allocated.
/// @param flags Allocation flags (e.g., GFP_KERNEL) passed to control memory
/// allocation behavior.
/// @return 0 on success, -1 on failure.
static int __alloc_slab_page(kmem_cache_t *cachep, gfp_t flags)
{
    // Allocate the required number of pages for the slab based on cache's
    // gfp_order. The higher the gfp_order, the more pages are allocated.
    page_t *page = _alloc_pages(flags, cachep->gfp_order);

    // Check if page allocation failed.
    if (!page) {
        pr_crit("Failed to allocate a new page from slab.\n");
        return -1;
    }

    // Initialize the linked lists for the slab page.
    // These lists track which objects in the page are free and which are in use.
    list_head_init(&page->slabs);         // Initialize the list of slabs (active objects).
    list_head_init(&page->slab_freelist); // Initialize the free list (unused objects).

    // Save a reference to the `kmem_cache_t` structure in the root page.
    // This is necessary for freeing arbitrary pointers and tracking cache ownership.
    page[0].container.slab_cache = cachep;

    // Update the slab main page pointer for all child pages (in case the allocation
    // consists of multiple pages) to point back to the root page.
    // This helps in reconstructing the main slab page when dealing with subpages.
    for (unsigned int i = 1; i < (1U << cachep->gfp_order); i++) {
        page[i].container.slab_main_page = page; // Link child pages to the main page.
    }

    // Calculate the total size of the slab (in bytes) by multiplying the page size
    // by the number of pages allocated (determined by the cache's gfp_order).
    unsigned int slab_size = PAGE_SIZE * (1U << cachep->gfp_order);

    // Update object counters for the page.
    // The total number of objects in the slab is determined by the slab size
    // divided by the size of each object in the cache.
    page->slab_objcnt  = slab_size / cachep->size; // Total number of objects.
    page->slab_objfree = page->slab_objcnt;        // Initially, all objects are free.

    // Get the starting physical address of the allocated slab page.
    unsigned int pg_addr = get_lowmem_address_from_page(page);

    // Check if `get_lowmem_address_from_page` failed.
    if (!pg_addr) {
        pr_crit("Failed to get low memory address for slab page.\n");
        return -1;
    }

    // Loop through each object in the slab and initialize its kmem_obj structure.
    // Each object is inserted into the free list, indicating that it is available.
    for (unsigned int i = 0; i < page->slab_objcnt; i++) {
        // Calculate the object's address by adding the offset (i * object size) to the page address.
        kmem_obj_t *obj = KMEM_OBJ_FROM_ADDR(pg_addr + cachep->size * i);

        // Insert the object into the slab's free list, making it available for allocation.
        list_head_insert_after(&obj->objlist, &page->slab_freelist);
    }

    // Insert the page into the cache's list of free slab pages.
    list_head_insert_after(&page->slabs, &cachep->slabs_free);

    // Update the cache's total object counters to reflect the new slab.
    cachep->total_num += page->slab_objcnt; // Increase the total number of objects in the cache.
    cachep->free_num += page->slab_objcnt;  // Increase the number of free objects.

    return 0;
}

static void __kmem_cache_refill(kmem_cache_t *cachep, unsigned int free_num, gfp_t flags)
{
    while (cachep->free_num < free_num) {
        if (__alloc_slab_page(cachep, flags) < 0) {
            pr_warning("Cannot allocate a page, abort refill\n");
            break;
        }
    }
}

static void __compute_size_and_order(kmem_cache_t *cachep)
{
    // Align the whole object to the required padding.
    cachep->size = round_up(
        max(cachep->object_size, KMEM_OBJ_OVERHEAD),
        max(8, cachep->align));
    // Compute the gfp order
    unsigned int size = round_up(cachep->size, PAGE_SIZE) / PAGE_SIZE;
    while ((size /= 2) > 0) {
        cachep->gfp_order++;
    }
}

static void __kmem_cache_create(
    kmem_cache_t *cachep,
    const char *name,
    unsigned int size,
    unsigned int align,
    slab_flags_t flags,
    kmem_fun_t ctor,
    kmem_fun_t dtor,
    unsigned int start_count)
{
    pr_info("Creating new cache `%s` with objects of size `%d`.\n", name, size);

    *cachep = (kmem_cache_t){
        .name        = name,
        .object_size = size,
        .align       = align,
        .flags       = flags,
        .ctor        = ctor,
        .dtor        = dtor
    };

    list_head_init(&cachep->slabs_free);
    list_head_init(&cachep->slabs_partial);
    list_head_init(&cachep->slabs_full);

    __compute_size_and_order(cachep);

    __kmem_cache_refill(cachep, start_count, flags);

    list_head_insert_after(&cachep->cache_list, &kmem_caches_list);
}

static inline void *__kmem_cache_alloc_slab(kmem_cache_t *cachep, page_t *slab_page)
{
    list_head *elem_listp = list_head_pop(&slab_page->slab_freelist);
    if (!elem_listp) {
        pr_warning("There are no FREE element inside the slab_freelist\n");
        return NULL;
    }
    slab_page->slab_objfree--;
    cachep->free_num--;
    kmem_obj_t *obj = list_entry(elem_listp, kmem_obj_t, objlist);
    if (!obj) {
        pr_warning("The kmem object is invalid\n");
        return NULL;
    }
    // Get the element from the kmem_obj object
    void *elem = ADDR_FROM_KMEM_OBJ(obj);
    if (cachep->ctor) {
        cachep->ctor(elem);
    }
    return elem;
}

static inline void __kmem_cache_free_slab(kmem_cache_t *cachep, page_t *slab_page)
{
    cachep->free_num -= slab_page->slab_objfree;
    cachep->total_num -= slab_page->slab_objcnt;
    // Clear objcnt, used as a flag to check if the page belongs to the slab
    slab_page->slab_objcnt              = 0;
    slab_page->container.slab_main_page = NULL;

    // Reset all non-root slab pages
    for (unsigned int i = 1; i < (1U << cachep->gfp_order); i++) {
        (slab_page + i)->container.slab_main_page = NULL;
    }

    __free_pages(slab_page);
}

void kmem_cache_init(void)
{
    // Initialize the list of caches.
    list_head_init(&kmem_caches_list);
    // Create a cache to store the data about caches.
    __kmem_cache_create(
        &kmem_cache,
        "kmem_cache_t",
        sizeof(kmem_cache_t),
        alignof(kmem_cache_t),
        GFP_KERNEL,
        NULL,
        NULL, 32);
    for (unsigned int i = 0; i < MAX_KMALLOC_CACHE_ORDER; i++) {
        malloc_blocks[i] = kmem_cache_create(
            "kmalloc",
            1u << i,
            1u << i,
            GFP_KERNEL,
            NULL,
            NULL);
    }
}

kmem_cache_t *kmem_cache_create(
    const char *name,
    unsigned int size,
    unsigned int align,
    slab_flags_t flags,
    kmem_fun_t ctor,
    kmem_fun_t dtor)
{
    kmem_cache_t *cachep = (kmem_cache_t *)kmem_cache_alloc(&kmem_cache, GFP_KERNEL);
    if (!cachep) {
        return cachep;
    }

    __kmem_cache_create(cachep, name, size, align, flags, ctor, dtor, KMEM_START_OBJ_COUNT);

    return cachep;
}

void kmem_cache_destroy(kmem_cache_t *cachep)
{
    while (!list_head_empty(&cachep->slabs_free)) {
        list_head *slab_list = list_head_pop(&cachep->slabs_free);
        __kmem_cache_free_slab(cachep, list_entry(slab_list, page_t, slabs));
    }

    while (!list_head_empty(&cachep->slabs_partial)) {
        list_head *slab_list = list_head_pop(&cachep->slabs_partial);
        __kmem_cache_free_slab(cachep, list_entry(slab_list, page_t, slabs));
    }

    while (!list_head_empty(&cachep->slabs_full)) {
        list_head *slab_list = list_head_pop(&cachep->slabs_full);
        __kmem_cache_free_slab(cachep, list_entry(slab_list, page_t, slabs));
    }

    kmem_cache_free(cachep);
    list_head_remove(&cachep->cache_list);
}

#ifdef ENABLE_CACHE_TRACE
void *pr_kmem_cache_alloc(const char *file, const char *fun, int line, kmem_cache_t *cachep, gfp_t flags)
#else
void *kmem_cache_alloc(kmem_cache_t *cachep, gfp_t flags)
#endif
{
    if (list_head_empty(&cachep->slabs_partial)) {
        if (list_head_empty(&cachep->slabs_free)) {
            if (flags == 0) {
                flags = cachep->flags;
            }
            // Refill the cache in an exponential fashion, capping at KMEM_MAX_REFILL_OBJ_COUNT to avoid
            // too big allocations
            __kmem_cache_refill(cachep, min(cachep->total_num, KMEM_MAX_REFILL_OBJ_COUNT), flags);
            if (list_head_empty(&cachep->slabs_free)) {
                pr_crit("Cannot allocate more slabs in `%s`\n", cachep->name);
                return NULL;
            }
        }

        // Add a free slab to partial list because in any case an element will
        // be removed before the function returns
        list_head *free_slab = list_head_pop(&cachep->slabs_free);
        if (!free_slab) {
            pr_crit("We retrieved an invalid slab from the free list.");
            return NULL;
        }
        list_head_insert_after(free_slab, &cachep->slabs_partial);
    }
    page_t *slab_page = list_entry(cachep->slabs_partial.next, page_t, slabs);
    if (!slab_page) {
        pr_crit("We retrieved an invalid slab from the partial list.");
        return NULL;
    }
    void *ptr = __kmem_cache_alloc_slab(cachep, slab_page);
    if (!ptr) {
        pr_crit("We failed to allocate a slab.");
        return NULL;
    }

    // If the slab is now full, add it to the full slabs list.
    if (slab_page->slab_objfree == 0) {
        list_head *slab_full_elem = list_head_pop(&cachep->slabs_partial);
        if (!slab_full_elem) {
            pr_crit("We retrieved an invalid slab from the partial list.");
            return NULL;
        }
        list_head_insert_after(slab_full_elem, &cachep->slabs_full);
    }
#ifdef ENABLE_CACHE_TRACE
    pr_notice("CHACE-ALLOC 0x%p in %-20s at %s:%d\n", ptr, cachep->name, file, line);
#endif
    return ptr;
}

#ifdef ENABLE_CACHE_TRACE
void pr_kmem_cache_free(const char *file, const char *fun, int line, void *ptr)
#else
void kmem_cache_free(void *ptr)
#endif
{
    page_t *slab_page = get_lowmem_page_from_address((uint32_t)ptr);

    // If the slab main page is a lowmem page, change to it as it's the root page
    if (is_lowmem_page_struct(slab_page->container.slab_main_page)) {
        slab_page = slab_page->container.slab_main_page;
    }

    kmem_cache_t *cachep = slab_page->container.slab_cache;

#ifdef ENABLE_CACHE_TRACE
    pr_notice("CHACE-FREE  0x%p in %-20s at %s:%d\n", ptr, cachep->name, file, line);
#endif
    if (cachep->dtor) {
        cachep->dtor(ptr);
    }

    kmem_obj_t *obj = KMEM_OBJ_FROM_ADDR(ptr);

    // Add object to the free list
    list_head_insert_after(&obj->objlist, &slab_page->slab_freelist);
    slab_page->slab_objfree++;
    cachep->free_num++;

    // Now page is completely free
    if (slab_page->slab_objfree == slab_page->slab_objcnt) {
        // Remove page from partial list
        list_head_remove(&slab_page->slabs);
        // Add page to free list
        list_head_insert_after(&slab_page->slabs, &cachep->slabs_free);
    }
    // Now page is not full, so change its list
    else if (slab_page->slab_objfree == 1) {
        // Remove page from full list
        list_head_remove(&slab_page->slabs);
        // Add page to partial list
        list_head_insert_after(&slab_page->slabs, &cachep->slabs_partial);
    }
}

#ifdef ENABLE_ALLOC_TRACE
void *pr_kmalloc(const char *file, const char *fun, int line, unsigned int size)
#else
void *kmalloc(unsigned int size)
#endif
{
    unsigned int order = 0;
    while (size != 0) {
        order++;
        size /= 2;
    }

    // If size does not fit in the maximum cache order, allocate raw pages
    void *ptr;
    if (order >= MAX_KMALLOC_CACHE_ORDER) {
        ptr = (void *)__alloc_pages_lowmem(GFP_KERNEL, order - 12);
    } else {
        ptr = kmem_cache_alloc(malloc_blocks[order], GFP_KERNEL);
    }
#ifdef ENABLE_ALLOC_TRACE
    pr_notice("KMALLOC 0x%p at %s:%d\n", ptr, file, line);
#endif
    return ptr;
}

#ifdef ENABLE_ALLOC_TRACE
void pr_kfree(const char *file, const char *fun, int line, void *ptr)
#else
void kfree(void *ptr)
#endif
{
#ifdef ENABLE_ALLOC_TRACE
    pr_notice("KFREE   0x%p at %s:%d\n", ptr, file, line);
#endif
    page_t *page = get_lowmem_page_from_address((uint32_t)ptr);

    // If the address is part of the cache
    if (page->container.slab_main_page) {
        kmem_cache_free(ptr);
    } else {
        free_pages_lowmem((uint32_t)ptr);
    }
}
