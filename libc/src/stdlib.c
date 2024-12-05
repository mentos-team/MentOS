/// @file stdlib.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stddef.h"
#include "assert.h"
#include "stdlib.h"
#include "string.h"
#include "stdint.h"
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
    if (header) {
        return (void *)((char *)header + sizeof(malloc_header_t));
    }
    return NULL;
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
    // Return NULL if size is zero, as no memory needs to be allocated.
    if (size == 0) {
        return NULL;
    }
    // Pointer for the allocated memory.
    void *ptr;
    // Attempt to allocate memory via a system call.
    __inline_syscall_1(ptr, brk, size + sizeof(malloc_header_t));
    // Check if the system call failed (e.g., due to lack of memory).
    if (ptr) {
        // Initialize the malloc header at the start of the allocated block.
        malloc_header_t *header = (malloc_header_t *)ptr;
        // Set the magic number to verify validity.
        header->magic = MALLOC_MAGIC_NUMBER;
        // Store the requested size.
        header->size = size;
        // Return a pointer to the memory block after the header.
        ptr = malloc_header_to_ptr(header);
    }
    return ptr;
}

void *calloc(size_t num, size_t size)
{
    // Check for overflow in multiplication (num * size)
    if ((num != 0) && ((size * num) > SIZE_MAX)) {
        return NULL;
    }
    // Allocate memory.
    void *ptr = malloc(num * size);
    if (ptr) {
        // Zero-initialize the allocated memory.
        memset(ptr, 0, num * size);
    }
    // Return the allocated and initialized memory.
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    // C standard implementation: When NULL is passed to realloc, simply malloc
    // the requested size and return a pointer to that.
    if (__builtin_expect(ptr == NULL, 0)) {
        return malloc(size);
    }
    // C standard implementation: For a size of zero, free the pointer and
    // return NULL, allocating no new memory.
    if (__builtin_expect(size == 0, 0)) {
        free(ptr);
        return NULL;
    }
    // Get the malloc header for the pointer.
    malloc_header_t *header = ptr_to_malloc_header(ptr);
    // Validate the header.
    if (!header || (header->magic != MALLOC_MAGIC_NUMBER)) {
        return NULL;
    }
    // Get the old size from the header.
    size_t old_size = header->size;
    // Allocate new memory for the resized block.
    void *newp = malloc(size);
    if (newp) {
        // Zero-initialize the new memory block.
        memset(newp, 0, size);
        // Copy the contents from the old memory block to the new one.
        memcpy(newp, ptr, old_size < size ? old_size : size);
        // Free the old memory block.
        free(ptr);
    }
    // Return the pointer to the new memory block.
    return newp;
}

void free(void *ptr)
{
    if (ptr) {
        // Get the malloc header.
        malloc_header_t *header = ptr_to_malloc_header(ptr);
        // Validate the malloc header.
        if (!header || (header->magic != MALLOC_MAGIC_NUMBER)) {
            return;
        }
        // Perform the system call to release memory.
        int _res;
        __inline_syscall_1(_res, brk, (char *)header);
    }
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
