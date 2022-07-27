/// @file slab.h
/// @brief Functions and structures for managing memory slabs.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "klib/list_head.h"
#include "stddef.h"
#include "mem/gfp.h"

/// @brief Type for slab flags.
typedef unsigned int slab_flags_t;

/// Create a new cache.
#define KMEM_CREATE(objtype) kmem_cache_create(#objtype,          \
                                               sizeof(objtype),   \
                                               alignof(objtype), \
                                               GFP_KERNEL,        \
                                               NULL,              \
                                               NULL)

/// Creates a new cache and allows to specify the constructor.
#define KMEM_CREATE_CTOR(objtype, ctor) kmem_cache_create(#objtype,                   \
                                                          sizeof(objtype),            \
                                                          alignof(objtype),          \
                                                          GFP_KERNEL,                 \
                                                          ((void (*)(void *))(ctor)), \
                                                          NULL)

/// @brief Stores the information of a cache.
typedef struct kmem_cache_t {
    /// Handler for placing it inside a lists of caches.
    list_head cache_list;
    /// Name of the cache.
    const char *name;
    /// Size of the cache.
    unsigned int size;
    /// Size of the objects contained in the cache.
    unsigned int object_size;
    /// Alignment requirement of the type of objects.
    unsigned int align;
    /// The total number of slabs.
    unsigned int total_num;
    /// The number of free slabs.
    unsigned int free_num;
    /// The Get Free Pages (GFP) flags.
    slab_flags_t flags;
    /// The order for getting free pages.
    unsigned int gfp_order;
    /// Constructor for the elements.
    void (*ctor)(void *);
    /// Destructor for the elements.
    void (*dtor)(void *);
    /// Handler for the full slabs list.
    list_head slabs_full;
    /// Handler for the partial slabs list.
    list_head slabs_partial;
    /// Handler for the free slabs list.
    list_head slabs_free;
} kmem_cache_t;

/// Initialize the slab system
void kmem_cache_init();

/// @brief Creates a new slab cache.
/// @param name  Name of the cache.
/// @param size  Size of the objects contained inside the cache.
/// @param align Memory alignment for the objects inside the cache.
/// @param flags Flags used to define the properties of the cache.
/// @param ctor  Constructor for initializing the cache elements.
/// @param dtor  Destructor for finalizing the cache elements.
/// @return Pointer to the object used to manage the cache.
kmem_cache_t *kmem_cache_create(
    const char *name,
    unsigned int size,
    unsigned int align,
    slab_flags_t flags,
    void (*ctor)(void *),
    void (*dtor)(void *));

/// @brief Deletes the given cache.
/// @param cachep Pointer to the cache.
void kmem_cache_destroy(kmem_cache_t *cachep);

#ifdef ENABLE_CACHE_TRACE

/// @brief Allocs a new object using the provided cache.
/// @param file   File where the object is allocated.
/// @param fun    Function where the object is allocated.
/// @param line   Line inside the file.
/// @param cachep The cache used to allocate the object.
/// @param flags  Flags used to define where we are going to Get Free Pages (GFP).
/// @return Pointer to the allocated space.
void *pr_kmem_cache_alloc(const char *file, const char *fun, int line, kmem_cache_t *cachep, gfp_t flags);

/// @brief Frees an cache allocated object.
/// @param file File where the object is deallocated.
/// @param fun  Function where the object is deallocated.
/// @param line Line inside the file.
/// @param addr Address of the object.
void pr_kmem_cache_free(const char *file, const char *fun, int line, void *addr);

/// Wrapper that provides the filename, the function and line where the alloc is happening.
#define kmem_cache_alloc(...) pr_kmem_cache_alloc(__FILE__, __func__, __LINE__, __VA_ARGS__)

/// Wrapper that provides the filename, the function and line where the free is happening.
#define kmem_cache_free(...) pr_kmem_cache_free(__FILE__, __func__, __LINE__, __VA_ARGS__)

#else
/// @brief Allocs a new object using the provided cache.
/// @param cachep The cache used to allocate the object.
/// @param flags  Flags used to define where we are going to Get Free Pages (GFP).
/// @return Pointer to the allocated space.
void *kmem_cache_alloc(kmem_cache_t *cachep, gfp_t flags);

/// @brief Frees an cache allocated object.
/// @param addr Address of the object.
void kmem_cache_free(void *addr);

#endif

#ifdef ENABLE_ALLOC_TRACE

/// @brief Provides dynamically allocated memory in kernel space.
/// @param file File where the object is allocated.
/// @param fun  Function where the object is allocated.
/// @param line Line inside the file.
/// @param size The amount of memory to allocate.
/// @return A pointer to the allocated memory.
void *pr_kmalloc(const char *file, const char *fun, int line, unsigned int size);

/// @brief Frees dynamically allocated memory in kernel space.
/// @param file File where the object is deallocated.
/// @param fun  Function where the object is deallocated.
/// @param line Line inside the file.
/// @param ptr The pointer to the allocated memory.
void pr_kfree(const char *file, const char *fun, int line, void *addr);

/// Wrapper that provides the filename, the function and line where the alloc is happening.
#define kmalloc(...) pr_kmalloc(__FILE__, __func__, __LINE__, __VA_ARGS__)

/// Wrapper that provides the filename, the function and line where the free is happening.
#define kfree(...) pr_kfree(__FILE__, __func__, __LINE__, __VA_ARGS__)

#else

/// @brief Provides dynamically allocated memory in kernel space.
/// @param size The amount of memory to allocate.
/// @return A pointer to the allocated memory.
void *kmalloc(unsigned int size);

/// @brief Frees dynamically allocated memory in kernel space.
/// @param ptr The pointer to the allocated memory.
void kfree(void *ptr);

#endif
