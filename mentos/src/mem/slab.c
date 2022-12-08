/// @file mouse.h
/// @brief  Driver for *PS2* Mouses.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[SLAB  ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "mem/zone_allocator.h"
#include "mem/paging.h"
#include "assert.h"
#include "io/debug.h"
#include "mem/slab.h"

/// @brief Use it to manage cached pages.
typedef struct kmem_obj {
    /// The list_head for this object.
    list_head objlist;
} kmem_obj;

/// Max order of kmalloc cache allocations, if greater raw page allocation is done.
#define MAX_KMALLOC_CACHE_ORDER 12

#define KMEM_OBJ_OVERHEAD                    sizeof(kmem_obj)
#define KMEM_START_OBJ_COUNT                 8
#define KMEM_MAX_REFILL_OBJ_COUNT            64
#define KMEM_OBJ(cachep, addr)               ((kmem_obj *)(addr))
#define ADDR_FROM_KMEM_OBJ(cachep, kmem_obj) ((void *)(kmem_obj))

// The list of caches.
static list_head kmem_caches_list;
// Cache where we will store the data about caches.
static kmem_cache_t kmem_cache;
// Caches for each order of the malloc.
static kmem_cache_t *malloc_blocks[MAX_KMALLOC_CACHE_ORDER];

static int __alloc_slab_page(kmem_cache_t *cachep, gfp_t flags)
{
    page_t *page = _alloc_pages(flags, cachep->gfp_order);
    if (!page) {
        pr_crit("Failed to allocate a new page from slab.\n");
        return -1;
    }

    list_head_init(&page->slabs);

    // Save in the root page the kmem_cache_t pointer,
    // to allow freeing arbitrary pointers
    page[0].container.slab_cache = cachep;

    // Update slab main pages of all child pages, to allow
    // reconstructing which page handles a specified address
    for (unsigned int i = 1; i < (1U << cachep->gfp_order); i++) {
        page[i].container.slab_main_page = page;
    }

    unsigned int slab_size = PAGE_SIZE * (1U << cachep->gfp_order);

    // Update the page objects counters
    page->slab_objcnt  = slab_size / cachep->size;
    page->slab_objfree = page->slab_objcnt;

    unsigned int pg_addr = get_lowmem_address_from_page(page);

    list_head_init(&page->slab_freelist);

    // Build the objects structures
    for (unsigned int i = 0; i < page->slab_objcnt; i++) {
        kmem_obj *obj = KMEM_OBJ(cachep, pg_addr + cachep->size * i);
        list_head_insert_after(&obj->objlist, &page->slab_freelist);
    }

    // Add the page to the slab list and update the counters
    list_head_insert_after(&page->slabs, &cachep->slabs_free);
    cachep->total_num += page->slab_objcnt;
    cachep->free_num += page->slab_objcnt;

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

static unsigned int __find_next_alignment(unsigned int size, unsigned int align)
{
    return (size / align + (size % align ? 1 : 0)) * align;
}

static void __compute_size_and_order(kmem_cache_t *cachep)
{
    // Align the whole object to the required padding
    cachep->size = __find_next_alignment(
        max(cachep->object_size, KMEM_OBJ_OVERHEAD),
        max(8, cachep->align));

    // Compute the gfp order
    unsigned int size = __find_next_alignment(cachep->size, PAGE_SIZE) / PAGE_SIZE;
    while ((size /= 2) > 0) {
        cachep->gfp_order++;
    }
}

static void __kmem_cache_create(kmem_cache_t *cachep, const char *name, unsigned int size, unsigned int align, slab_flags_t flags, void (*ctor)(void *), void (*dtor)(void *), unsigned int start_count)
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

    kmem_obj *obj = list_entry(elem_listp, kmem_obj, objlist);

    // Get the element from the kmem_obj object
    void *elem = ADDR_FROM_KMEM_OBJ(cachep, obj);

    if (cachep->ctor)
        cachep->ctor(elem);

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

void kmem_cache_init()
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

kmem_cache_t *kmem_cache_create(const char *name, unsigned int size, unsigned int align, slab_flags_t flags, void (*ctor)(void *), void (*dtor)(void *))
{
    kmem_cache_t *cachep = (kmem_cache_t *)kmem_cache_alloc(&kmem_cache, GFP_KERNEL);
    if (!cachep)
        return cachep;

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
            if (flags == 0)
                flags = cachep->flags;

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
        list_head_insert_after(free_slab, &cachep->slabs_partial);
    }

    page_t *slab_page = list_entry(cachep->slabs_partial.next, page_t, slabs);
    void *ptr         = __kmem_cache_alloc_slab(cachep, slab_page);

    // If the slab is now full, add it to the full slabs list
    if (slab_page->slab_objfree == 0) {
        list_head *slab_full_elem = list_head_pop(&cachep->slabs_partial);
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
    if (cachep->dtor)
        cachep->dtor(ptr);

    kmem_obj *obj = KMEM_OBJ(cachep, ptr);

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
