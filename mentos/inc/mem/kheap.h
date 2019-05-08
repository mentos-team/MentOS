///                MentOS, The Mentoring Operating system project
/// @file kheap.h
/// @brief  Interface for kernel heap functions, also provides a placement
///         malloc() for use before the heap is initialised.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "kernel.h"
#include "scheduler.h"

//#define KHEAP_START         (void*)(LOAD_MEMORY_ADDRESS + 0x00800000) // 0x00800000 = 8M
//#define KHEAP_MAX_ADDRESS   (void*)(LOAD_MEMORY_ADDRESS + 0x0FFFFFFF) // 0x10000000 = 256 M
// M = 1024 * 1024 = 1MB
#define KHEAP_INITIAL_SIZE (4 * M)
#define UHEAP_INITIAL_SIZE (1 * M)

/**
 * @brief Structure of block_t
 *  size: |            31 bit             |   1 bit    |
 *        |    first bits of real size    | free/alloc |
 *        To calculate the real size, set to zero the last bit
 *  prev: -> block_t
 *  next: -> block_t
 **/
typedef struct block_t {
	unsigned int size;
	struct block_t *nextfree;
	struct block_t *next;
} block_t;

/// @brief              Initialize heap.
/// @param initial_size Starting size.
void kheap_init(size_t initial_size);

/// @brief Returns if the kheap is enabled.
bool_t kheap_is_enabled();

void *ksbrk(int increment);

#if 0
/// @brief Kmalloc wrapper.
/// @details When heap is not created, use a placement memory allocator, when
/// heap is created, use malloc(), the dynamic memory allocator.
/// @param size  Size of memory to allocate.
/// @param align Return a page-aligned memory block.
/// @param phys  Return the physical address of the memory block.
/// @return
uint32_t kmalloc_int(size_t size, bool_t align, uint32_t *phys);

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
#endif

/// @brief      Dynamically allocates memory of the given size.
/// @param sz   Size of memory to allocate.
/// @return
void *kmalloc(unsigned int sz);

/// @brief      Wrapper for aligned kmalloc.
/// @param size Size of memory to allocate.
/// @return
void *kmalloc_align(size_t size);

/// @brief Dynamically allocates memory for (size * num) and memset it.
/// @param num  Multiplier.
/// @param size Size of memory to allocate.
/// @return
void *kcalloc(unsigned int num, unsigned int size);

/// @brief      Wrapper function for realloc.
/// @param ptr
/// @param size
/// @return
void *krealloc(void *ptr, unsigned int size);

/// @brief Wrapper function for free.
/// @param p
void kfree(void *p);

/// @brief Allocates size bytes of uninitialized storage.
//void *malloc(unsigned int size);

// We can't do a user heap brk if we don't enable paging...
void *usbrk(int increment);

/// @brief User malloc
/// @param size The size requested in byte.
/// @return The address of the allocated space.
void *umalloc(unsigned int size);

/// @brief   User free
/// @param p Pointer to the space to free.
void ufree(void *p);

/// @brief Allocates size bytes of uninitialized storage with block align.
//void *malloc_align(struct vm_area_struct *heap, unsigned int size);

/// @brief Deallocates previously allocated space.
//void free(void *ptr);

/// @brief      Reallocates the given area of memory. It must be still allocated
///             and not yet freed with a call to free or realloc.
/// @param ptr
/// @param size
/// @return
//void *realloc(void *ptr, unsigned int size);

/// @brief Prints the heap visually, for debug.
void kheap_dump();

/// @brief Get the pointer to the kernel heap start.
/// @return The kernel heap start pointer.
uint32_t get_kheap_start();

/// @brief Get the pointer to the kernel heap curr.
/// @return The kernel heap current pointer.
uint32_t get_kheap_curr();
