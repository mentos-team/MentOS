/// @file stdlib.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stddef.h"
#include "assert.h"
#include "stdlib.h"
#include "string.h"
#include "system/syscall_types.h"

/// @brief Number which identifies a memory area allocated through a call to
/// malloc(), calloc() or realloc().
#define MALLOC_MAGIC_NUMBER 0x600DC0DE

/// @brief A structure that holds the information about an allocated chunk of
/// memory through malloc.
typedef struct {
    /// @brief A magic number that is used to check if the passed pointer is
    /// actually a malloc allocated memory.
    unsigned magic;
    /// @brief The size of the allocated memory, useful when doing a realloc.
    size_t size;
} malloc_header_t;

/// @brief Extract the actual pointer to the allocated memory from the malloc header.
/// @param header the header we are using.
/// @return a pointer to the allocated memory.
static inline void *malloc_header_to_ptr(malloc_header_t *header)
{
    return (void *)((char *)header + sizeof(malloc_header_t));
}

/// @brief Extract the malloc header, from the actual pointer to the allocated memory.
/// @param ptr the pointer we use.
/// @return the malloc header.
static inline malloc_header_t *ptr_to_malloc_header(void *ptr)
{
    return (malloc_header_t *)((char *)ptr - sizeof(malloc_header_t));
}

void *malloc(unsigned int size)
{
    assert(size && "Zero size requested.");
    size_t *ptr;
    // Compute the real size we need to allocate.
    size_t real_size = size + sizeof(malloc_header_t);
    // Allocate the memory.
    __inline_syscall1(ptr, brk, real_size);
    // Initialize the malloc header.
    malloc_header_t *malloc_header = (malloc_header_t *)ptr;
    malloc_header->magic           = MALLOC_MAGIC_NUMBER;
    malloc_header->size            = size;
    // Return the allocated memory.
    return (void *)((char *)ptr + sizeof(malloc_header_t));
}

void *calloc(size_t num, size_t size)
{
    void *ptr = malloc(num * size);
    if (ptr) {
        memset(ptr, 0, num * size);
    }
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    // C standard implementation: When NULL is passed to realloc,
    // simply malloc the requested size and return a pointer to that.
    if (__builtin_expect(ptr == NULL, 0)) {
        return malloc(size);
    }
    // C standard implementation: For a size of zero, free the
    // pointer and return NULL, allocating no new memory.
    if (__builtin_expect(size == 0, 0)) {
        free(ptr);
        return NULL;
    }
    // Get the malloc header.
    malloc_header_t *malloc_header = (malloc_header_t *)((char *)ptr - sizeof(malloc_header_t));
    // Check the header.
    assert(malloc_header->magic == MALLOC_MAGIC_NUMBER && "This is not a valid pointer.");
    // Get the old size.
    size_t old_size = malloc_header->size;
    // Create the new pointer.
    void *newp = malloc(size);
    memset(newp, 0, size);
    memcpy(newp, ptr, old_size);
    free(ptr);
    return newp;
}

void free(void *ptr)
{
    // Get the malloc header.
    malloc_header_t *malloc_header = (malloc_header_t *)((char *)ptr - sizeof(malloc_header_t));
    // Check the header.
    assert(malloc_header->magic == MALLOC_MAGIC_NUMBER && "This is not a valid pointer.");
    // Get the real pointer.
    ptr = (char *)ptr - sizeof(malloc_header_t);
    // Call the free.
    int _res;
    __inline_syscall1(_res, brk, ptr);
}

/// Seed used to generate random numbers.
static unsigned rseed = 0;

void srand(unsigned x)
{
    rseed = x;
}

unsigned rand(void)
{
    return rseed = (rseed * 1103515245U + 12345U) & RAND_MAX;
}

float randf(void)
{
    return ((float)rand() / (float)(RAND_MAX));
}

int randint(int lb, int ub)
{
    return lb + ((int)rand() % (ub - lb + 1));
}

unsigned randuint(unsigned lb, unsigned ub)
{
    return lb + (rand() % (ub - lb + 1));
}

float randfloat(float lb, float ub)
{
    return lb + (randf() * (ub - lb));
}
