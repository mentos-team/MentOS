/// @file stdlib.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/syscall_types.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"

/// @brief Number which identifies a memory area allocated through a call to
/// malloc(), calloc() or realloc().
#define MALLOC_MAGIC_NUMBER 0x600DC0DE

/// @brief Checks if the pointer is a valid malloc entry.
/// @param ptr the pointer we are checking.
/// @return 1 of success, 0 on failure.
static inline int __malloc_is_valid_ptr(void *ptr)
{
    return (ptr && (((size_t *)ptr)[-1] == MALLOC_MAGIC_NUMBER));
}

size_t malloc_usable_size(void *ptr)
{
    if (__malloc_is_valid_ptr(ptr))
        return ((size_t *)ptr)[-2];
    return 0;
}

void *malloc(unsigned int size)
{
    size_t *_res, _size = 2 * sizeof(size_t) + size;
    __inline_syscall1(_res, brk, _size);
    _res[0] = size;
    _res[1] = MALLOC_MAGIC_NUMBER;
    return &_res[2];
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
    // Get the old size.
    size_t old_size = malloc_usable_size(ptr);
    // Create the new pointer.
    void *newp = malloc(size);
    memset(newp, 0, size);
    memcpy(newp, ptr, old_size);
    free(ptr);
    return newp;
}

void free(void *ptr)
{
    int _res;
    if (__malloc_is_valid_ptr(ptr)) {
        size_t *_ptr = (size_t *)ptr - 2;
        __inline_syscall1(_res, brk, _ptr);
    } else {
        __inline_syscall1(_res, brk, ptr);
    }
}

/// Seed used to generate random numbers.
static int rseed = 0;

void srand(int x)
{
    rseed = x;
}

int rand()
{
    return rseed = (rseed * 1103515245U + 12345U) & RAND_MAX;
}
