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
#include "resource_tracing.h"

#ifdef ENABLE_KMEM_TRACE
/// @brief Tracks the unique ID of the currently registered resource.
static int resource_id = -1;
#endif

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
static inline int __alloc_slab_page(kmem_cache_t *cachep, gfp_t flags)
{
    // Validate input parameter.
    if (!cachep) {
        pr_crit("Invalid cache pointer (NULL), cannot allocate slab page.\n");
        return -1;
    }

    // Allocate the required number of pages for the slab based on cache's `gfp_order`.
    page_t *page = alloc_pages(flags, cachep->gfp_order);

    // Check if page allocation failed.
    if (!page) {
        pr_crit("Failed to allocate a new page for cache `%s`.\n", cachep->name);
        return -1;
    }

    // Initialize the linked lists for the slab page.
    list_head_init(&page->slabs);         // Initialize the list of slabs (active objects).
    list_head_init(&page->slab_freelist); // Initialize the free list (unused objects).

    // Save a reference to the `kmem_cache_t` structure in the root page.
    page[0].container.slab_cache = cachep;

    // Update the slab main page pointer for all child pages.
    for (unsigned i = 1; i < (1U << cachep->gfp_order); i++) {
        page[i].container.slab_main_page = page; // Link child pages to the main page.
    }

    // Calculate the total size of the slab (in bytes).
    unsigned slab_size = PAGE_SIZE * (1U << cachep->gfp_order);

    // Update object counters for the page.
    page->slab_objcnt  = slab_size / cachep->aligned_object_size; // Total number of objects.
    page->slab_objfree = page->slab_objcnt;                       // Initially, all objects are free.

    // Get the starting virtual address of the allocated slab page.
    unsigned pg_addr = get_virtual_address_from_page(page);

    // Check if `get_virtual_address_from_page` failed.
    if (!pg_addr) {
        pr_crit("Failed to get virtual address for slab page in cache `%s`.\n", cachep->name);
        // Free allocated pages before returning.
        if (free_pages(page) < 0) {
            pr_crit(
                "Failed to free allocated pages before returning in cache "
                "`%s`.\n",
                cachep->name);
        }
        return -1;
    }

    // Initialize each object in the slab and insert it into the free list.
    for (unsigned i = 0; i < page->slab_objcnt; i++) {
        // Calculate the object's address.
        kmem_obj_t *obj = KMEM_OBJ_FROM_ADDR(pg_addr + cachep->aligned_object_size * i);

        // Insert the object into the slab's free list.
        list_head_insert_after(&obj->objlist, &page->slab_freelist);
    }

    // Insert the page into the cache's list of free slab pages.
    list_head_insert_after(&page->slabs, &cachep->slabs_free);

    // Update the cache's total object counters.
    cachep->total_num += page->slab_objcnt; // Increase the total number of objects.
    cachep->free_num += page->slab_objcnt;  // Increase the number of free objects.

    pr_debug("Allocated slab page with %u objects for cache `%s`.\n", page->slab_objcnt, cachep->name);
    return 0;
}

/// @brief Refills a memory cache with new slab pages to reach a specified number of free objects.
/// @details This function allocates new slab pages as needed until the cache has at least `free_num` free objects.
/// If a page allocation fails, the refill process is aborted.
/// @param cachep Pointer to the memory cache (`kmem_cache_t`) that needs to be refilled.
/// @param free_num The desired number of free objects in the cache.
/// @param flags Allocation flags used for controlling memory allocation behavior (e.g., GFP_KERNEL).
/// @return 0 on success, -1 on failure.
static int __kmem_cache_refill(kmem_cache_t *cachep, unsigned free_num, gfp_t flags)
{
    // Check for a valid cache pointer.
    if (!cachep) {
        pr_crit("Invalid cache pointer (NULL), cannot refill.\n");
        return -1;
    }

    // Continue allocating slab pages until the cache has at least `free_num`
    // free objects.
    while (cachep->free_num < free_num) {
        // Attempt to allocate a new slab page. If allocation fails, print a
        // warning and abort the refill process.
        if (__alloc_slab_page(cachep, flags) < 0) {
            pr_crit(
                "Failed to allocate a new slab page for cache `%s`, aborting "
                "refill.\n",
                cachep->name);
            return -1; // Return -1 if page allocation fails.
        }
    }

    pr_debug("Successfully refilled cache `%s` to have at least %u free objects.\n", cachep->name, free_num);
    return 0;
}

/// @brief Computes and sets the size and gfp order for a memory cache.
/// @details This function adjusts the size of objects in the cache based on
/// padding and alignment requirements, and calculates the `gfp_order` (number
/// of contiguous pages) needed for slab allocations.
/// @param cachep Pointer to the memory cache (`kmem_cache_t`) whose size and
/// order are being computed.
/// @return 0 on success, -1 on failure.
static int __compute_size_and_order(kmem_cache_t *cachep)
{
    // Validate inputs.
    if (!cachep) {
        pr_crit("Invalid cache pointer (NULL).\n");
        return -1;
    }
    if (cachep->raw_object_size == 0) {
        pr_crit("Object size is invalid (0), cannot compute cache size and "
                "order.\n");
        return -1;
    }
    if (cachep->align == 0) {
        pr_crit("Alignment is invalid (0), cannot compute cache size and order.\n");
        return -1;
    }

    // Align the object size to the required padding.
    // The object size is padded based on either the `KMEM_OBJ_OVERHEAD` or the
    // provided alignment requirement. Ensure that the object size is at least
    // as large as the overhead and is aligned to the cache's alignment.
    cachep->aligned_object_size = round_up(
        max(cachep->raw_object_size,
            KMEM_OBJ_OVERHEAD), // Ensure object size is larger than the overhead.
        max(8,
            cachep->align)); // Ensure alignment is at least 8 bytes for proper memory alignment.

    // Check if the computed size is valid.
    if (cachep->aligned_object_size == 0) {
        pr_crit("Computed object size is zero; invalid for cache allocation.\n");
        return -1;
    }

    // Compute the `gfp_order` based on the total object size and page size.
    // The `gfp_order` determines how many contiguous pages will be allocated
    // for the slab.
    unsigned int size = round_up(cachep->aligned_object_size, PAGE_SIZE) / PAGE_SIZE;

    // Reset `gfp_order` to 0 before calculating.
    cachep->gfp_order = 0;

    // Calculate the order by determining how many divisions by 2 the size
    // undergoes until it becomes smaller than or equal to 1.
    while ((size /= 2) > 0) {
        cachep->gfp_order++;
    }

    // Check for a valid `gfp_order`. Ensure that it's within reasonable limits.
    if (cachep->gfp_order > MAX_BUDDYSYSTEM_GFP_ORDER) {
        pr_crit(
            "Calculated gfp_order (%u) exceeds system limit (%u); limiting to "
            "max.\n",
            cachep->gfp_order, MAX_BUDDYSYSTEM_GFP_ORDER);
        cachep->gfp_order = MAX_BUDDYSYSTEM_GFP_ORDER;
    }

    // Additional consistency check (optional):
    // Verify that the calculated gfp_order leads to a valid page allocation size.
    if ((cachep->gfp_order == 0) && (cachep->aligned_object_size > PAGE_SIZE)) {
        pr_crit("gfp_order is 0 but object size exceeds one page; issue in "
                "size calculation.\n");
        return -1;
    }

    pr_debug(
        "Computed aligned object size `%u` and gfp_order `%u` for cache "
        "`%s`.\n",
        cachep->aligned_object_size, cachep->gfp_order, cachep->name);

    return 0;
}

/// @brief Initializes and creates a new memory cache.
/// @details This function sets up a new memory cache (`kmem_cache_t`) with the provided parameters such as
/// object size, alignment, constructor/destructor functions, and flags. It also initializes slab lists,
/// computes the appropriate size and order, refills the cache with objects, and adds it to the global cache list.
/// @param cachep Pointer to the memory cache structure to initialize.
/// @param name Name of the cache.
/// @param size Size of the objects to be allocated from the cache.
/// @param align Alignment requirement for the objects.
/// @param flags Slab allocation flags.
/// @param ctor Constructor function to initialize objects (optional, can be NULL).
/// @param dtor Destructor function to clean up objects (optional, can be NULL).
/// @param start_count Initial number of objects to populate in the cache.
/// @return 0 on success, -1 on failure.
static int __kmem_cache_create(
    kmem_cache_t *cachep,     // Pointer to the cache structure to be created.
    const char *name,         // Name of the cache.
    unsigned int size,        // Size of the objects in the cache.
    unsigned int align,       // Object alignment.
    slab_flags_t flags,       // Allocation flags.
    kmem_fun_t ctor,          // Constructor function for cache objects.
    kmem_fun_t dtor,          // Destructor function for cache objects.
    unsigned int start_count) // Initial number of objects to populate in the cache.
{
    // Log the creation of a new cache.
    pr_info("Creating new cache `%s` with objects of size `%u`.\n", name, size);

    // Input validation checks.
    if (!cachep) {
        pr_crit("Invalid cache pointer (NULL), cannot create cache.\n");
        return -1;
    }
    if (!name || size == 0) {
        pr_crit("Invalid cache name or object size (size = %u).\n", size);
        return -1;
    }

    // Set up the basic properties of the cache.
    *cachep = (kmem_cache_t){
        // .cache_list          = 0,
        .name = name,  .aligned_object_size = 0, .raw_object_size = size, .align = align, .total_num = 0,
        .free_num = 0, .flags = flags,           .gfp_order = 0,          .ctor = ctor,   .dtor = dtor,
        // .slabs_full          = 0,
        // .slabs_partial       = 0,
        // .slabs_free          = 0,
    };

    // Initialize the list heads for free, partial, and full slabs.
    list_head_init(&cachep->slabs_free);
    list_head_init(&cachep->slabs_partial);
    list_head_init(&cachep->slabs_full);

    // Compute the object size and gfp_order for slab allocations.
    // Validate that size and order are computed successfully.
    if (__compute_size_and_order(cachep) < 0) {
        pr_crit("Failed to compute size and order for cache `%s`.\n", name);
        return -1;
    }

    // Refill the cache with `start_count` objects.
    // If the refill fails, log a critical warning.
    if (__kmem_cache_refill(cachep, start_count, flags) < 0) {
        pr_crit("Failed to refill cache `%s` with initial objects.\n", name);
        return -1;
    }

    // Insert the cache into the global list of caches.
    list_head_insert_after(&cachep->cache_list, &kmem_caches_list);

    pr_debug("Successfully created cache `%s`.\n", name);
    return 0;
}

/// @brief Allocates an object from a specified slab page.
/// @details This function retrieves a free object from the given slab page's free list.
/// It decrements the count of free objects in both the slab page and the cache.
/// If the constructor function is defined, it will be called to initialize the object.
/// @param cachep Pointer to the cache from which the object is being allocated.
/// @param slab_page Pointer to the slab page from which to allocate the object.
/// @return Pointer to the allocated object, or NULL if allocation fails.
static inline void *__kmem_cache_alloc_slab(kmem_cache_t *cachep, page_t *slab_page)
{
    // Validate input parameters.
    if (!cachep) {
        pr_crit("Invalid cache pointer (NULL).\n");
        return NULL;
    }
    if (!slab_page) {
        pr_crit("Invalid slab_page pointer (NULL).\n");
        return NULL;
    }

    // Retrieve and remove the first element from the slab's free list.
    list_head *elem_listp = list_head_pop(&slab_page->slab_freelist);

    // Check if the free list is empty.
    if (!elem_listp) {
        pr_crit("No free elements in slab freelist for cache `%s`.\n", cachep->name);
        return NULL;
    }

    // Decrement the count of free objects in the slab page and the cache.
    if (slab_page->slab_objfree == 0 || cachep->free_num == 0) {
        pr_crit("Free object count underflow detected for cache `%s`.\n", cachep->name);
        return NULL;
    }

    slab_page->slab_objfree--;
    cachep->free_num--;

    // Get the kmem object from the list entry.
    kmem_obj_t *object = list_entry(elem_listp, kmem_obj_t, objlist);

    // Check if the kmem object pointer is valid.
    if (!object) {
        pr_crit("Invalid kmem object in cache `%s`.\n", cachep->name);
        return NULL;
    }

    // Get the address of the allocated element from the kmem object.
    void *elem = ADDR_FROM_KMEM_OBJ(object);

    // Call the constructor function if it is defined to initialize the object.
    if (cachep->ctor) {
        cachep->ctor(elem);
    }

    pr_debug("Successfully allocated object 0x%p from cache `%s`.\n", elem, cachep->name);

    return elem;
}

/// @brief Frees a slab page and updates the associated cache statistics.
/// @details This function updates the total and free object counts in the cache
/// and resets the slab page's metadata to indicate that it is no longer in use.
/// It also frees the memory associated with the slab page.
/// @param cachep Pointer to the cache from which the slab page is being freed.
/// @param slab_page Pointer to the slab page to be freed.
/// @return Returns 0 on success, or -1 if an error occurs.
static inline int __kmem_cache_free_slab(kmem_cache_t *cachep, page_t *slab_page)
{
    // Validate input parameters.
    if (!cachep) {
        pr_crit("Invalid cache pointer (NULL).\n");
        return -1;
    }
    if (!slab_page) {
        pr_crit("Invalid slab_page pointer (NULL).\n");
        return -1;
    }

    // Ensure cache object count is not underflowing
    if (cachep->free_num < slab_page->slab_objfree || cachep->total_num < slab_page->slab_objcnt) {
        pr_crit("Object count inconsistency detected in cache `%s`.\n", cachep->name);
        return -1;
    }

    // Update the free and total object counts in the cache.
    cachep->free_num -= slab_page->slab_objfree;
    cachep->total_num -= slab_page->slab_objcnt;

    // Clear the object count and reset the main page pointer as a flag to
    // indicate the page is no longer active.
    slab_page->slab_objcnt              = 0;
    slab_page->container.slab_main_page = NULL;

    // Reset the main page pointers for all non-root slab pages. This loop
    // assumes the first page is the root and resets pointers for child pages.
    for (unsigned i = 1; i < (1U << cachep->gfp_order); i++) {
        // Clear main page pointer for each child page.
        (slab_page + i)->container.slab_main_page = NULL;
    }

    // Free the memory associated with the slab page.
    if (free_pages(slab_page) < 0) {
        pr_crit("Failed to free slab page memory for cache `%s`.\n", cachep->name);
        return -1;
    }

    pr_debug("Successfully freed slab page for cache `%s`.\n", cachep->name);

    return 0;
}

int kmem_cache_init(void)
{
    // Initialize the list of caches to keep track of all memory caches.
    list_head_init(&kmem_caches_list);

#ifdef ENABLE_KMEM_TRACE
    ENABLE_EXT2_TRACE = register_resource("kmem");
#endif

    // Create a cache to store metadata about kmem_cache_t structures.
    if (__kmem_cache_create(
            &kmem_cache, "kmem_cache_t", sizeof(kmem_cache_t), alignof(kmem_cache_t), GFP_KERNEL, NULL, NULL, 32) < 0) {
        pr_crit("Failed to create kmem_cache for kmem_cache_t.\n");
        return -1;
    }

    // Create caches for different order sizes for kmalloc allocations.
    for (unsigned i = 0; i < MAX_KMALLOC_CACHE_ORDER; i++) {
        malloc_blocks[i] = kmem_cache_create(
            "kmalloc",
            1u << i, // Size of the allocation (2^i).
            1u << i, // Alignment of the allocation.
            GFP_KERNEL,
            NULL,  // Constructor (none).
            NULL); // Destructor (none).

        // Check if the cache was created successfully.
        if (!malloc_blocks[i]) {
            pr_crit("Failed to create kmalloc cache for order %u.\n", i);

            // Clean up any previously allocated caches before exiting.
            for (unsigned j = 0; j < i; j++) {
                if (malloc_blocks[j]) {
                    if (kmem_cache_destroy(malloc_blocks[j]) < 0) {
                        pr_crit("Failed to destroy kmalloc cache for order %u.\n", j);
                    }
                    malloc_blocks[j] = NULL;
                }
            }
            // Destroy kmem_cache to free resources.
            if (kmem_cache_destroy(&kmem_cache) < 0) {
                pr_crit("Failed to destroy kmem_cache to free resources %u.\n", i);
            }
            return -1;
        }
    }

    pr_info("kmem_cache system successfully initialized.\n");

    return 0;
}

kmem_cache_t *kmem_cache_create(
    const char *name,
    unsigned int size,
    unsigned int align,
    slab_flags_t flags,
    kmem_fun_t ctor,
    kmem_fun_t dtor)
{
    // Check for a valid cache name.
    if (!name || !*name) {
        pr_crit("Invalid cache name provided.\n");
        return NULL;
    }

    // Check for a valid cache size.
    if (size == 0) {
        pr_crit("Cache size must be greater than zero.\n");
        return NULL;
    }

    // Allocate memory for a new kmem_cache_t structure.
    kmem_cache_t *cachep = (kmem_cache_t *)kmem_cache_alloc(&kmem_cache, GFP_KERNEL);

    // Check if memory allocation for the cache failed.
    if (!cachep) {
        pr_crit("Failed to allocate memory for kmem_cache_t.\n");
        return NULL;
    }

    // Initialize the kmem_cache_t structure.
    if (__kmem_cache_create(cachep, name, size, align, flags, ctor, dtor, KMEM_START_OBJ_COUNT) < 0) {
        pr_crit("Failed to initialize kmem_cache for '%s'.\n", name);

        // Free allocated memory if initialization fails.
        if (kmem_cache_free(cachep) < 0) {
            pr_crit("Failed to free allocated memory for '%s'.\n", name);
        }

        return NULL;
    }

    pr_debug("Successfully created cache '%s'.\n", name);

    return cachep;
}

int kmem_cache_destroy(kmem_cache_t *cachep)
{
    // Validate input parameter.
    if (!cachep) {
        pr_crit("Cannot destroy a NULL cache pointer.\n");
        return -1;
    }

    // Free all slabs in the free list.
    while (!list_head_empty(&cachep->slabs_free)) {
        list_head *slab_list = list_head_pop(&cachep->slabs_free);
        if (!slab_list) {
            pr_crit("Failed to retrieve a slab from free list.\n");
            return -1;
        }
        __kmem_cache_free_slab(cachep, list_entry(slab_list, page_t, slabs));
    }

    // Free all slabs in the partial list.
    while (!list_head_empty(&cachep->slabs_partial)) {
        list_head *slab_list = list_head_pop(&cachep->slabs_partial);
        if (!slab_list) {
            pr_crit("Failed to retrieve a slab from partial list.\n");
            return -1;
        }
        __kmem_cache_free_slab(cachep, list_entry(slab_list, page_t, slabs));
    }

    // Free all slabs in the full list.
    while (!list_head_empty(&cachep->slabs_full)) {
        list_head *slab_list = list_head_pop(&cachep->slabs_full);
        if (!slab_list) {
            pr_crit("Failed to retrieve a slab from full list.\n");
            return -1;
        }
        __kmem_cache_free_slab(cachep, list_entry(slab_list, page_t, slabs));
    }

    // Free the cache structure itself.
    if (kmem_cache_free(cachep) != 0) {
        pr_crit("Failed to free cache structure.\n");
        return -1;
    }

    // Remove the cache from the global cache list.
    list_head_remove(&cachep->cache_list);

    pr_debug("Successfully destroyed cache `%s`.\n", cachep->name);

    return 0;
}

void *pr_kmem_cache_alloc(const char *file, const char *fun, int line, kmem_cache_t *cachep, gfp_t flags)
{
    // Check for null cache pointer
    if (!cachep) {
        pr_err("Null cache pointer provided.\n");
        return NULL;
    }

    // Check if there are any partially filled slabs.
    if (list_head_empty(&cachep->slabs_partial)) {
        // If no partial slabs, check for free slabs.
        if (list_head_empty(&cachep->slabs_free)) {
            // If no flags are specified, use the cache's flags.
            if (flags == 0) {
                flags = cachep->flags;
            }

            // Attempt to refill the cache, limiting the number of objects.
            if (__kmem_cache_refill(cachep, min(cachep->total_num, KMEM_MAX_REFILL_OBJ_COUNT), flags) < 0) {
                pr_crit("Failed to refill cache `%s`\n", cachep->name);
                return NULL;
            }

            // If still no free slabs, log an error and return NULL.
            if (list_head_empty(&cachep->slabs_free)) {
                pr_crit("Cannot allocate more slabs in `%s`\n", cachep->name);
                return NULL;
            }
        }

        // Move a free slab to the partial list since we're about to allocate from it.
        list_head *free_slab = list_head_pop(&cachep->slabs_free);
        if (!free_slab) {
            pr_crit("Retrieved invalid slab from free list.\n");
            return NULL;
        }
        list_head_insert_after(free_slab, &cachep->slabs_partial);
    }

    // Retrieve the slab page from the partial list.
    page_t *slab_page = list_entry(cachep->slabs_partial.next, page_t, slabs);
    if (!slab_page) {
        pr_crit("Retrieved invalid slab from partial list.\n");
        return NULL;
    }

    // Allocate an object from the slab page.
    void *ptr = __kmem_cache_alloc_slab(cachep, slab_page);
    if (!ptr) {
        pr_crit("Failed to allocate object from slab.\n");
        return NULL;
    }

    // If the slab is now full, move it to the full slabs list.
    if (slab_page->slab_objfree == 0) {
        list_head *slab_full_elem = list_head_pop(&cachep->slabs_partial);
        if (!slab_full_elem) {
            pr_crit("Retrieved invalid slab from partial list while moving to "
                    "full list.\n");
            return NULL;
        }
        list_head_insert_after(slab_full_elem, &cachep->slabs_full);
    }

#if defined(ENABLE_CACHE_TRACE) || (__DEBUG_LEVEL__ >= LOGLEVEL_DEBUG)
    pr_notice("kmem_cache_alloc 0x%p in %-20s at %s:%d\n", ptr, cachep->name, file, line);
#endif

    return ptr; // Return pointer to the allocated object.
}

int pr_kmem_cache_free(const char *file, const char *fun, int line, void *addr)
{
    // Check for null pointer input
    if (!addr) {
        pr_crit("Null pointer provided.\n");
        return 1;
    }

    // Get the slab page corresponding to the given pointer.
    page_t *slab_page = get_page_from_virtual_address((uint32_t)addr);

    // Check if slab_page retrieval was successful
    if (!slab_page) {
        pr_crit("Failed to get slab page for pointer 0x%p.\n", addr);
        return 1;
    }

    // If the slab main page is a low memory page, update to the root page.
    if (is_lowmem_page_struct(slab_page->container.slab_main_page)) {
        slab_page = slab_page->container.slab_main_page;
    }

    // Retrieve the cache pointer from the slab page.
    kmem_cache_t *cachep = slab_page->container.slab_cache;

    // Check if cachep retrieval was successful
    if (!cachep) {
        pr_crit("Failed to retrieve cache from slab page for pointer 0x%p.\n", addr);
        return 1;
    }

#if defined(ENABLE_CACHE_TRACE) || (__DEBUG_LEVEL__ >= LOGLEVEL_DEBUG)
    pr_notice("kmem_cache_free  0x%p in %-20s at %s:%d\n", addr, cachep->name, file, line);
#endif

    // Call the destructor if defined.
    if (cachep->dtor) {
        cachep->dtor(addr);
    }

    // Get the kmem_obj from the pointer.
    kmem_obj_t *obj = KMEM_OBJ_FROM_ADDR(addr);

    // Check if obj retrieval was successful
    if (!obj) {
        pr_crit("Failed to retrieve kmem object for pointer 0x%p.\n", addr);
        return 1;
    }

    // Add object to the free list of the slab.
    list_head_insert_after(&obj->objlist, &slab_page->slab_freelist);
    slab_page->slab_objfree++;
    cachep->free_num++;

    // Check if the slab is completely free, move it to the free list.
    if (slab_page->slab_objfree == slab_page->slab_objcnt) {
        // Remove the page from the partial list.
        list_head_remove(&slab_page->slabs);
        // Add the page to the free list.
        list_head_insert_after(&slab_page->slabs, &cachep->slabs_free);
        pr_debug("Slab page 0x%p moved to free list.\n", slab_page);
    }
    // If the page is not full, update its list status.
    else if (slab_page->slab_objfree == 1) {
        // Remove the page from the full list.
        list_head_remove(&slab_page->slabs);
        // Add the page to the partial list.
        list_head_insert_after(&slab_page->slabs, &cachep->slabs_partial);
        pr_debug("Slab page 0x%p moved to partial list.\n", slab_page);
    }
    return 0;
}

void *pr_kmalloc(const char *file, const char *fun, int line, unsigned int size)
{
    unsigned int order = 0;

    // Determine the order based on the size requested.
    while (size != 0) {
        order++;
        size /= 2;
    }

    // Allocate memory. If size exceeds the maximum cache order, allocate raw pages.
    void *ptr;
    if (order >= MAX_KMALLOC_CACHE_ORDER) {
        ptr = (void *)alloc_pages_lowmem(GFP_KERNEL, order - 12);
        if (!ptr) {
            pr_crit("Failed to allocate raw pages for order %u at %s:%d\n", order, file, line);
        }
    } else {
        ptr = kmem_cache_alloc(malloc_blocks[order], GFP_KERNEL);
        if (!ptr) {
            pr_crit(
                "Failed to allocate from kmalloc cache order %u for size %u at "
                "%s:%d\n",
                order, size, file, line);
        }
    }

#ifdef ENABLE_KMEM_TRACE
    if (ptr) {
        pr_notice("kmalloc 0x%p of order %u at %s:%d\n", ptr, order, file, line);
    }
    store_resource_info(resource_id, file, line, ptr);
#endif
    return ptr;
}

void pr_kfree(const char *file, const char *fun, int line, void *ptr)
{
    if (!ptr) {
        pr_warning("Attempt to free NULL pointer at %s:%d\n", file, line);
        return;
    }

    // Get the slab page from the pointer's address.
    page_t *page = get_page_from_virtual_address((uint32_t)ptr);

    // Check if page retrieval was successful.
    if (!page) {
        pr_crit("Failed to retrieve page for address 0x%p at %s:%d\n", ptr, file, line);
        return;
    }

    // If the address belongs to a cache, free it using kmem_cache_free.
    if (page->container.slab_main_page) {
        if (kmem_cache_free(ptr) < 0) {
            pr_crit(
                "Failed to free memory from kmem_cache for address 0x%p at "
                "%s:%d\n",
                ptr, file, line);
        }
    } else {
        // Otherwise, free the raw pages.
        if (free_pages_lowmem((uint32_t)ptr) < 0) {
            pr_crit("Failed to free raw pages for address 0x%p at %s:%d\n", ptr, file, line);
        }
    }

#ifdef ENABLE_KMEM_TRACE
    pr_notice("kfree   0x%p at %s:%d\n", ptr, file, line);
    clear_resource_info(ptr);
    print_resource_usage(resource_id, NULL);
#endif
}
