/// @file slab.h
/// @brief Functions and structures for managing memory slabs.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list_head.h"
#include "stddef.h"
#include "mem/gfp.h"

/// @brief Type for slab flags.
typedef unsigned int slab_flags_t;

/// @brief Type of function used as constructor/destructor for cache creation and destruction.
typedef void (*kmem_fun_t)(void *);

/// Create a new cache.
#define KMEM_CREATE(objtype) \
    kmem_cache_create(#objtype, sizeof(objtype), alignof(objtype), GFP_KERNEL, NULL, NULL)

/// Creates a new cache and allows to specify the constructor.
#define KMEM_CREATE_CTOR(objtype, ctor) \
    kmem_cache_create(#objtype, sizeof(objtype), alignof(objtype), GFP_KERNEL, (kmem_fun_t)(ctor), NULL)

/// @brief Stores the information of a cache.
typedef struct kmem_cache_t {
    /// Link to place this cache in a global list of caches.
    list_head cache_list;
    /// Name of the cache.
    const char *name;
    /// Total size of each object in the cache, including alignment and padding.
    unsigned int aligned_object_size;
    /// Original, unaligned size of the objects requested by the user.
    unsigned int raw_object_size;
    /// Alignment requirement for objects in the cache.
    unsigned int align;
    /// Total number of objects allocated across all slabs.
    unsigned int total_num;
    /// Number of free objects available across all slabs.
    unsigned int free_num;
    /// Flags for page allocation behavior.
    slab_flags_t flags;
    /// Page allocation order (power of 2 pages) used for slab allocation.
    unsigned int gfp_order;
    /// Constructor function for initializing objects.
    kmem_fun_t ctor;
    /// Destructor function for cleaning up objects.
    kmem_fun_t dtor;
    /// List of fully occupied slabs.
    list_head slabs_full;
    /// List of partially occupied slabs.
    list_head slabs_partial;
    /// List of completely free slabs.
    list_head slabs_free;
} kmem_cache_t;

/// @brief Initializes the kernel memory cache system.
/// @details This function initializes the global cache list and creates the
/// main cache for managing kmem_cache_t structures. It also creates caches for
/// different order sizes for kmalloc allocations.
/// @note This function should be called during system initialization.
/// @return Returns 0 on success, or -1 if an error occurs.
int kmem_cache_init(void);

/// @brief Creates a new kmem_cache structure.
/// @details This function allocates memory for a new cache and initializes it
/// with the provided parameters. The cache is ready for use after this function
/// returns.
/// @param name Name of the cache.
/// @param size Size of each object in the cache.
/// @param align Alignment requirement for objects in the cache.
/// @param flags Flags for slab allocation.
/// @param ctor Constructor function for initializing objects (can be NULL).
/// @param dtor Destructor function for cleaning up objects (can be NULL).
/// @return Pointer to the newly created kmem_cache_t, or NULL if allocation
/// fails.
kmem_cache_t *kmem_cache_create(
    const char *name,
    unsigned int size,
    unsigned int align,
    slab_flags_t flags,
    kmem_fun_t ctor,
    kmem_fun_t dtor);

/// @brief Destroys a specified kmem_cache structure.
/// @details This function cleans up and frees all memory associated with the
/// specified cache, including all associated slab pages. After calling this
/// function, the cache should no longer be used.
/// @param cachep Pointer to the kmem_cache_t structure to destroy.
/// @return Returns 0 on success, or -1 if an error occurs.
int kmem_cache_destroy(kmem_cache_t *cachep);

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
/// @return 0 on success, 1 on error.
int pr_kmem_cache_free(const char *file, const char *fun, int line, void *addr);

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
#define kmem_cache_alloc(...) pr_kmem_cache_alloc(__FILE__, __func__, __LINE__, __VA_ARGS__)

/// Wrapper that provides the filename, the function and line where the free is happening.
#define kmem_cache_free(...) pr_kmem_cache_free(__FILE__, __func__, __LINE__, __VA_ARGS__)

/// Wrapper that provides the filename, the function and line where the alloc is happening.
#define kmalloc(...) pr_kmalloc(__FILE__, __func__, __LINE__, __VA_ARGS__)

/// Wrapper that provides the filename, the function and line where the free is happening.
#define kfree(...) pr_kfree(__FILE__, __func__, __LINE__, __VA_ARGS__)
