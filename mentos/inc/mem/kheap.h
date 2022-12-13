/// @file kheap.h
/// @brief  Interface for kernel heap functions, also provides a placement
///         malloc() for use before the heap is initialised.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "kernel.h"
#include "process/scheduler.h"

/// @brief Initialize heap.
/// @param initial_size Starting size.
void kheap_init(size_t initial_size);

/// @brief Increase the heap of the kernel.
/// @param increment The amount to increment.
/// @return Pointer to a ready-to-used memory area.
void *ksbrk(int increment);

/// @brief Increase the heap of the current process.
/// @param increment The amount to increment.
/// @return Pointer to a ready-to-used memory area.
void *usbrk(int increment);

/// @brief User malloc.
/// @param addr This argument is treated as an address of a dynamically
///             allocated memory if falls inside the process heap area.
///             Otherwise, it is treated as an amount of memory that
///             should be allocated.
/// @return NULL if there is no more memory available or we were freeing
///         a previously allocated memory area, the address of the
///         allocated space otherwise.
void *sys_brk(void *addr);

/// @brief Prints the heap visually, for debug.
void kheap_dump();

#if 0
/// @brief Kmalloc wrapper.
/// @details When heap is not created, use a placement memory allocator, when
/// heap is created, use malloc(), the dynamic memory allocator.
/// @param size  Size of memory to allocate.
/// @param align Return a page-aligned memory block.
/// @param phys  Return the physical address of the memory block.
/// @return
uint32_t kmalloc_int(size_t size, int align, uint32_t *phys);

/// @brief Wrapper for kmalloc, get physical address.
/// @param sz   Size of memory to allocate.
/// @param phys Pointer to the variable where to place the physical address.
/// @return
void *kmalloc_p(unsigned int sz, unsigned int *phys);

/// @brief Wrapper for aligned kmalloc, get physical address.
/// @param sz   Size of memory to allocate.
/// @param phys Pointer to the variable where to place the physical address.
/// @return
void *kmalloc_ap(unsigned int sz, unsigned int *phys);

/// @brief      Dynamically allocates memory of the given size.
/// @param sz   Size of memory to allocate.
/// @return
//void *kmalloc(unsigned int sz);

/// @brief      Wrapper for aligned kmalloc.
/// @param size Size of memory to allocate.
/// @return
//void *kmalloc_align(size_t size);

/// @brief Dynamically allocates memory for (size * num) and memset it.
/// @param num  Multiplier.
/// @param size Size of memory to allocate.
/// @return
//void *kcalloc(unsigned int num, unsigned int size);

/// @brief      Wrapper function for realloc.
/// @param ptr
/// @param size
/// @return
//void *krealloc(void *ptr, unsigned int size);

/// @brief Wrapper function for free.
/// @param p
//void kfree(void *p);

/// @brief Allocates size bytes of uninitialized storage.
//void *malloc(unsigned int size);

/// @brief Allocates size bytes of uninitialized storage with block align.
//void *malloc_align(vm_area_struct_t *heap, unsigned int size);

/// @brief Deallocates previously allocated space.
//void free(void *ptr);

/// @brief      Reallocates the given area of memory. It must be still allocated
///             and not yet freed with a call to free or realloc.
/// @param ptr
/// @param size
/// @return
//void *realloc(void *ptr, unsigned int size);

#endif
