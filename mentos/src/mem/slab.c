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

/// @brief Refills a memory cache with new slab pages to reach a specified number of free objects.
/// @details This function allocates new slab pages as needed until the cache has at least `free_num` free objects.
/// If a page allocation fails, the refill process is aborted.
/// @param cachep Pointer to the memory cache (`kmem_cache_t`) that needs to be refilled.
/// @param free_num The desired number of free objects in the cache.
/// @param flags Allocation flags used for controlling memory allocation behavior (e.g., GFP_KERNEL).
/// @return 0 on success, -1 on failure.
static int __kmem_cache_refill(kmem_cache_t *cachep, unsigned int free_num, gfp_t flags)
{
    // Continue allocating slab pages until the cache has at least `free_num`
    // free objects.
    while (cachep->free_num < free_num) {
        // Attempt to allocate a new slab page. If allocation fails, print a
        // warning and abort the refill process.
        if (__alloc_slab_page(cachep, flags) < 0) {
            pr_crit("Failed to allocate a new slab page, aborting refill\n");
            return -1; // Return -1 if page allocation fails.
        }
    }
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
    // Check for invalid or uninitialized object sizes or alignment.
    // If `object_size` or `align` is zero, the cache cannot be correctly
    // configured.
    if (cachep->object_size == 0) {
        pr_crit("Object size is invalid (0), cannot compute cache size and order.\n");
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
    cachep->size = round_up(
        max(cachep->object_size, KMEM_OBJ_OVERHEAD), // Ensure object size is larger than the overhead.
        max(8, cachep->align));                      // Ensure alignment is at least 8 bytes for proper memory alignment.

    // Check if the computed size is valid.
    if (cachep->size == 0) {
        pr_crit("Computed object size is invalid (0), cannot proceed with cache allocation.\n");
        return -1;
    }

    // Compute the `gfp_order` based on the total object size and page size.
    // The `gfp_order` determines how many contiguous pages will be allocated
    // for the slab.
    unsigned int size = round_up(cachep->size, PAGE_SIZE) / PAGE_SIZE;

    // Reset `gfp_order` to 0 before calculating.
    cachep->gfp_order = 0;

    // Calculate the order by determining how many divisions by 2 the size
    // undergoes until it becomes smaller than or equal to 1.
    while ((size /= 2) > 0) {
        cachep->gfp_order++;
    }

    // Check for a valid `gfp_order`. Ensure that it's within reasonable limits.
    if (cachep->gfp_order > MAX_BUDDYSYSTEM_GFP_ORDER) {
        pr_crit("Calculated gfp_order exceeds system limits (%d).\n", MAX_BUDDYSYSTEM_GFP_ORDER);
        cachep->gfp_order = MAX_BUDDYSYSTEM_GFP_ORDER;
    }

    // Additional consistency check (optional):
    // Verify that the calculated gfp_order leads to a valid page allocation size.
    if ((cachep->gfp_order == 0) && (cachep->size > PAGE_SIZE)) {
        pr_crit("Calculated gfp_order is 0, but object size exceeds one page. Potential issue in size computation.\n");
        return -1;
    }
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
    pr_info("Creating new cache `%s` with objects of size `%d`.\n", name, size);

    // Input validation checks.
    if (!cachep) {
        pr_crit("Invalid cache pointer (NULL), cannot create cache.\n");
        return -1;
    }
    if (!name || size == 0) {
        pr_crit("Invalid cache name or object size (size = %d).\n", size);
        return -1;
    }

    // Set up the basic properties of the cache.
    *cachep = (kmem_cache_t){
        .name        = name,
        .object_size = size,
        .align       = align,
        .flags       = flags,
        .ctor        = ctor,
        .dtor        = dtor
    };

    // Initialize the list heads for free, partial, and full slabs.
    list_head_init(&cachep->slabs_free);
    list_head_init(&cachep->slabs_partial);
    list_head_init(&cachep->slabs_full);

    // Compute the object size and gfp_order for slab allocations.
    // If there's an issue with size or order computation, this function should handle it internally.
    __compute_size_and_order(cachep);

    // Refill the cache with `start_count` objects.
    // If the refill fails (due to slab page allocation failure), a warning is logged.
    __kmem_cache_refill(cachep, start_count, flags);

    // Insert the cache into the global list of caches.
    // No error check needed here as list operations usually don't fail.
    list_head_insert_after(&cachep->cache_list, &kmem_caches_list);

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
    // Retrieve and remove the first element from the slab's free list.
    list_head *elem_listp = list_head_pop(&slab_page->slab_freelist);

    // Check if the free list is empty.
    if (!elem_listp) {
        pr_crit("There are no FREE elements inside the slab_freelist\n");
        return NULL; // Return NULL if no free elements are available.
    }

    // Decrement the count of free objects in the slab page and the cache.
    slab_page->slab_objfree--;
    cachep->free_num--;

    // Get the kmem object from the list entry.
    kmem_obj_t *object = list_entry(elem_listp, kmem_obj_t, objlist);

    // Check if the kmem object pointer is valid.
    if (!object) {
        pr_crit("The kmem object is invalid\n");
        return NULL;
    }

    // Get the address of the allocated element from the kmem object.
    void *elem = ADDR_FROM_KMEM_OBJ(object);

    // Call the constructor function if it is defined to initialize the object.
    if (cachep->ctor) {
        cachep->ctor(elem);
    }

    return elem; // Return the pointer to the allocated object.
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
    if (!cachep || !slab_page) {
        pr_crit("Invalid cache or slab_page pointer (NULL).\n");
        return -1; // Return error if either pointer is NULL.
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
    for (unsigned int i = 1; i < (1U << cachep->gfp_order); i++) {
        // Clear main page pointer for each child page.
        (slab_page + i)->container.slab_main_page = NULL;
    }

    // Free the memory associated with the slab page.
    __free_pages(slab_page);

    return 0; // Return success.
}

int kmem_cache_init(void)
{
    // Initialize the list of caches to keep track of all memory caches.
    list_head_init(&kmem_caches_list);

    // Create a cache to store metadata about kmem_cache_t structures.
    if (__kmem_cache_create(
            &kmem_cache,
            "kmem_cache_t",
            sizeof(kmem_cache_t),
            alignof(kmem_cache_t),
            GFP_KERNEL,
            NULL,
            NULL,
            32) < 0) {
        pr_crit("Failed to create kmem_cache for kmem_cache_t.\n");
        return -1; // Early exit if kmem_cache creation fails.
    }

    // Create caches for different order sizes for kmalloc allocations.
    for (unsigned int i = 0; i < MAX_KMALLOC_CACHE_ORDER; i++) {
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
            return -1; // Early exit if kmem_cache creation fails.
        }
    }

    return 0; // Return success.
}

kmem_cache_t *kmem_cache_create(
    const char *name,
    unsigned int size,
    unsigned int align,
    slab_flags_t flags,
    kmem_fun_t ctor,
    kmem_fun_t dtor)
{
    // Allocate memory for a new kmem_cache_t structure.
    kmem_cache_t *cachep = (kmem_cache_t *)kmem_cache_alloc(&kmem_cache, GFP_KERNEL);

    // Check if memory allocation for the cache failed.
    if (!cachep) {
        pr_crit("Failed to allocate memory for kmem_cache_t.\n");
        return NULL; // Return NULL to indicate failure.
    }

    // Initialize the kmem_cache_t structure.
    if (__kmem_cache_create(cachep, name, size, align, flags, ctor, dtor, KMEM_START_OBJ_COUNT) < 0) {
        pr_crit("Failed to initialize kmem_cache for '%s'.\n", name);
        // Free allocated memory if initialization fails.
        kmem_cache_free(cachep);
        return NULL; // Return NULL to indicate failure.
    }

    return cachep; // Return the pointer to the newly created cache.
}

int kmem_cache_destroy(kmem_cache_t *cachep)
{
    // Validate input parameter.
    if (!cachep) {
        pr_crit("Cannot destroy a NULL cache pointer.\n");
        return -1; // Early exit if cache pointer is NULL.
    }

    // Free all slabs in the free list.
    while (!list_head_empty(&cachep->slabs_free)) {
        list_head *slab_list = list_head_pop(&cachep->slabs_free);
        __kmem_cache_free_slab(cachep, list_entry(slab_list, page_t, slabs));
    }

    // Free all slabs in the partial list.
    while (!list_head_empty(&cachep->slabs_partial)) {
        list_head *slab_list = list_head_pop(&cachep->slabs_partial);
        __kmem_cache_free_slab(cachep, list_entry(slab_list, page_t, slabs));
    }

    // Free all slabs in the full list.
    while (!list_head_empty(&cachep->slabs_full)) {
        list_head *slab_list = list_head_pop(&cachep->slabs_full);
        __kmem_cache_free_slab(cachep, list_entry(slab_list, page_t, slabs));
    }

    // Free the cache structure itself.
    kmem_cache_free(cachep);
    // Remove the cache from the global cache list.
    list_head_remove(&cachep->cache_list);

    return 0; // Return success.
}

#ifdef ENABLE_CACHE_TRACE
void *pr_kmem_cache_alloc(const char *file, const char *fun, int line, kmem_cache_t *cachep, gfp_t flags)
#else
void *kmem_cache_alloc(kmem_cache_t *cachep, gfp_t flags)
#endif
{
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
                pr_crit("Failed to refill cache in `%s`\n", cachep->name);
                return NULL; // Return NULL to indicate failure.
            }
            // If still no free slabs, log an error and return NULL.
            if (list_head_empty(&cachep->slabs_free)) {
                pr_crit("Cannot allocate more slabs in `%s`\n", cachep->name);
                return NULL; // Return NULL to indicate failure.
            }
        }

        // Move a free slab to the partial list since we're about to allocate from it.
        list_head *free_slab = list_head_pop(&cachep->slabs_free);
        if (!free_slab) {
            pr_crit("We retrieved an invalid slab from the free list.");
            return NULL; // Return NULL to indicate failure.
        }
        list_head_insert_after(free_slab, &cachep->slabs_partial);
    }

    // Retrieve the slab page from the partial list.
    page_t *slab_page = list_entry(cachep->slabs_partial.next, page_t, slabs);
    if (!slab_page) {
        pr_crit("We retrieved an invalid slab from the partial list.");
        return NULL; // Return NULL to indicate failure.
    }

    // Allocate an object from the slab page.
    void *ptr = __kmem_cache_alloc_slab(cachep, slab_page);
    if (!ptr) {
        pr_crit("We failed to allocate a slab.");
        return NULL; // Return NULL to indicate failure.
    }

    // If the slab is now full, move it to the full slabs list.
    if (slab_page->slab_objfree == 0) {
        list_head *slab_full_elem = list_head_pop(&cachep->slabs_partial);
        if (!slab_full_elem) {
            pr_crit("We retrieved an invalid slab from the partial list.");
            return NULL; // Return NULL to indicate failure.
        }
        list_head_insert_after(slab_full_elem, &cachep->slabs_full);
    }

#ifdef ENABLE_CACHE_TRACE
    pr_notice("CACHE-ALLOC 0x%p in %-20s at %s:%d\n", ptr, cachep->name, file, line);
#endif
    return ptr; // Return pointer to the allocated object.
}

#ifdef ENABLE_CACHE_TRACE
void pr_kmem_cache_free(const char *file, const char *fun, int line, void *ptr)
#else
void kmem_cache_free(void *ptr)
#endif
{
    // Get the slab page corresponding to the given pointer.
    page_t *slab_page = get_lowmem_page_from_address((uint32_t)ptr);

    // If the slab main page is a low memory page, update to the root page.
    if (is_lowmem_page_struct(slab_page->container.slab_main_page)) {
        slab_page = slab_page->container.slab_main_page;
    }

    // Retrieve the cache pointer from the slab page.
    kmem_cache_t *cachep = slab_page->container.slab_cache;

#ifdef ENABLE_CACHE_TRACE
    pr_notice("CACHE-FREE  0x%p in %-20s at %s:%d\n", ptr, cachep->name, file, line);
#endif
    // Call the destructor if defined.
    if (cachep->dtor) {
        cachep->dtor(ptr);
    }

    // Get the kmem_obj from the pointer.
    kmem_obj_t *obj = KMEM_OBJ_FROM_ADDR(ptr);

    // Add object to the free list of the slab.
    list_head_insert_after(&obj->objlist, &slab_page->slab_freelist);
    slab_page->slab_objfree++;
    cachep->free_num++;

    // If the slab is completely free, move it to the free list.
    if (slab_page->slab_objfree == slab_page->slab_objcnt) {
        // Remove the page from the partial list.
        list_head_remove(&slab_page->slabs);
        // Add the page to the free list.
        list_head_insert_after(&slab_page->slabs, &cachep->slabs_free);
    }
    // If the page is not full, update its list status.
    else if (slab_page->slab_objfree == 1) {
        // Remove the page from the full list.
        list_head_remove(&slab_page->slabs);
        // Add the page to the partial list.
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

    // Determine the order based on the size requested.
    while (size != 0) {
        order++;
        size /= 2;
    }

    // Allocate memory. If size exceeds the maximum cache order, allocate raw pages.
    void *ptr;
    if (order >= MAX_KMALLOC_CACHE_ORDER) {
        ptr = (void *)__alloc_pages_lowmem(GFP_KERNEL, order - 12);
    } else {
        ptr = kmem_cache_alloc(malloc_blocks[order], GFP_KERNEL);
    }

#ifdef ENABLE_ALLOC_TRACE
    pr_notice("KMALLOC 0x%p at %s:%d\n", ptr, file, line);
#endif
    return ptr; // Return pointer to the allocated memory.
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
    // Get the slab page from the pointer's address.
    page_t *page = get_lowmem_page_from_address((uint32_t)ptr);

    // If the address belongs to a cache, free it using kmem_cache_free.
    if (page->container.slab_main_page) {
        kmem_cache_free(ptr);
    } else {
        // Otherwise, free the raw pages.
        free_pages_lowmem((uint32_t)ptr);
    }
}
