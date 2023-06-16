/// @file kheap.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/bitops.h"
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[KHEAP ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "mem/kheap.h"
#include "mem/paging.h"
#include "sys/list_head.h"
#include "stdbool.h"
#include "stdint.h"
#include "string.h"
#include "assert.h"
#include "math.h"

/// Overhead given by the block_t itself.
#define OVERHEAD sizeof(block_t)
/// Align the given address.
#define ADDR_ALIGN(addr) ((((uint32_t)(addr)) & 0xFFFFF000) + 0x1000)
/// Checks if the given address is aligned.
#define IS_ALIGN(addr) ((((uint32_t)(addr)) & 0x00000FFF) == 0)
/// User heap initial size ( 1 Megabyte).
#define UHEAP_INITIAL_SIZE (1 * M)

/// @brief Identifies a block of memory.
typedef struct block_t {
    /// @brief Identifies the side of the block and also if it is free or allocated.
    /// @details
    ///  |            31 bit             |   1 bit    |
    ///  |    first bits of real size    | free/alloc |
    ///  To calculate the real size, set to zero the last bit
    unsigned int size;
    /// Pointer to the next free block.
    struct block_t *nextfree;
    /// Pointer to the next block.
    struct block_t *next;
} block_t;

/// @brief Maps the heap memory to this three easily accessible values.
typedef struct {
    /// @brief Pointer to the head block.
    block_t *head;
    /// @brief Pointer to the tail block.
    block_t *tail;
    /// @brief Pointer to the free block list.
    block_t *free;
} heap_header_t;

/// Kernel heap section.
static vm_area_struct_t kernel_heap;
/// Top of the kernel heap.
static uint32_t kernel_heap_top;

/// @brief The size is encoded in the lowest 31 bits, while the 32nd is used to
/// determine if the block is free/used.
/// @param size the size of the block.
/// @return the real size of the block.
static inline uint32_t __blkmngr_get_real_size(uint32_t size)
{
    return bit_clear(size, 0);
}

/// @brief Returns the given size, rounded in multiples of 16.
/// @param size the given size.
/// @return the size rounded to the nearest multiple of 16.
static inline uint32_t __blkmngr_get_rounded_size(uint32_t size)
{
    return round_up(size, 16);
}

/// @brief Sets the free/used bit of the size field.
/// @param size the size field we want to set free/used.
/// @param control if 0 the block is free, otherwise is set as used.
static inline void __blkmngr_set_free(block_t *block, int control)
{
    assert(block && "Received a NULL block.");
    block->size = bit_toggle(block->size, 0, control);
}

/// @brief Checks if a block is freed or allocated.
static inline unsigned __blkmngr_is_free(block_t *block)
{
    return block && bit_check(block->size, 0);
}

/// @brief       Checks if the given size fits inside the block.
/// @param block The given block.
/// @param size  The size to check
/// @return
static inline int __blkmngr_does_it_fit(block_t *block, uint32_t size)
{
    assert(block && "Received null block.");
    return (block->size >= __blkmngr_get_real_size(size)) && __blkmngr_is_free(block);
}

static inline void __blkmngr_dump(heap_header_t *header)
{
    assert(header && "Received a NULL heap header.");
    if (header->head) {
        pr_warning("LIST: ");
        for (block_t *it = header->head; it; it = it->nextfree)
            pr_warning("0x%p:%2d ", it, it ? it->size : -1);
        pr_warning("\n");
    }
    if (header->free) {
        pr_warning("FREE: ");
        for (block_t *it = header->free; it; it = it->nextfree)
            pr_warning("0x%p:%2d ", it, it ? it->size : -1);
        pr_warning("\n");
    }
}

/// @brief Removes the block from freelist.
static inline void __blkmngr_remove_from_freelist(heap_header_t *header, block_t *block)
{
    assert(header && "Received a NULL heap header.");
    assert(block && "Received null block.");
    if (block == header->free) {
        header->free = block->nextfree;
    } else {
        block_t *prev = header->free;
        while (prev != NULL && prev->nextfree != block)
            prev = prev->nextfree;
        if (prev) {
            prev->nextfree = block->nextfree;
        }
    }
    block->nextfree = NULL;
}

/// @brief Add the block to the free list.
static inline void __blkmngr_add_to_freelist(heap_header_t *header, block_t *block)
{
    assert(header && "Received a NULL heap header.");
    assert(block && "Received null block.");
    block->nextfree = header->free;
    header->free    = block;
}

/// @brief Find the best fitting block in the memory pool.
/// @param header header describing the heap.
/// @param size the size we want.
/// @return a block that should fit our needs.
static inline block_t *__blkmngr_find_best_fitting(heap_header_t *header, uint32_t size)
{
    assert(header && "Received a NULL heap header.");
    block_t *best_fitting = NULL, *current;
    for (current = header->free; current; current = current->nextfree) {
        if (!__blkmngr_does_it_fit(current, size)) {
            continue;
        }
        if ((best_fitting == NULL) || (current->size < best_fitting->size)) {
            best_fitting = current;
        }
    }
    return best_fitting;
}

/// @brief Given a block, finds its previous block.
static inline block_t *__blkmngr_get_previous_block(heap_header_t *header, block_t *block)
{
    assert(header && "Received a NULL heap header.");
    assert(block && "Received null block.");
    // If the block is actually the head of the list, return NULL.
    if (block == header->head)
        return NULL;
    block_t *prev = header->head;
    // FIXME: Sometimes enters infinite loop!
    while (prev->next != block) {
        prev = prev->next;
    }
    return prev;
}

/// @brief Given a block, finds its next block.
static inline block_t *__blkmngr_get_next_block(heap_header_t *header, block_t *block)
{
    assert(header && "Received a NULL heap header.");
    assert(block && "Received null block.");
    // If the block is actually the tail of the list, return NULL.
    if (block == header->tail)
        return NULL;
    return block->next;
}

/// @brief Extends the provided heap of the given increment.
/// @param heap_top  Current top of the heap.
/// @param heap      Pointer to the heap.
/// @param increment Increment to the heap.
/// @return Pointer to the old top of the heap, ready to be used.
static void *__do_brk(uint32_t *heap_top, vm_area_struct_t *heap, uint32_t increment)
{
    assert(heap_top && "Pointer to the current top of the heap is NULL.");
    assert(heap && "Pointer to the heap is NULL.");
    uint32_t new_heap_top = *heap_top + increment, old_heap_top = *heap_top;
    // If new boundary is larger than the end, we fail.
    if (new_heap_top > heap->vm_end)
        return NULL;
    // Overwrite the top of the heap.
    *heap_top = new_heap_top;
    pr_debug("[%s] free space: %lu\n",
             (heap == &kernel_heap) ? "KERNEL" : " USER ",
             heap->vm_end - ((uint32_t)heap_top / M));
    // Return the old top of the heap.
    return (void *)old_heap_top;
}

/// @brief Allocates size bytes of uninitialized storage.
/// @param heap Heap from which we get the unallocated memory.
/// @param size Size of the desired memory area.
/// @return Pointer to the allocated memory area.
static void *__do_malloc(vm_area_struct_t *heap, size_t size)
{
    if (size == 0)
        return NULL;
    // Get the heap header.
    heap_header_t *header = (heap_header_t *)heap->vm_start;

    // Calculate real size that's used, round it to multiple of 16.
    uint32_t rounded_size = __blkmngr_get_rounded_size(size);
    // The block size takes into account also the block_t overhead.
    uint32_t block_size = rounded_size + OVERHEAD;
    // Find bestfit in avl tree. This bestfit function will remove the
    // best-fit node when there is more than one such node in tree.
    block_t *best_fitting = __blkmngr_find_best_fitting(header, rounded_size);
    // If we have found the best fitting block, use it.
    if (best_fitting) {
        // Store the base pointer.
        block_t *base_ptr = best_fitting;
        // Store a pointer to the next block.
        block_t *stored_next = __blkmngr_get_next_block(header, best_fitting);
        // Get the size of the chunk.
        uint32_t chunk_size = __blkmngr_get_real_size(best_fitting->size) + OVERHEAD;
        // Get what's left.
        uint32_t remaining_size = chunk_size - block_size;
        // Get the real size.
        uint32_t real_size = (remaining_size < (8 + OVERHEAD)) ? chunk_size : block_size;
        // Set the size of the best fitting block.
        best_fitting->size = real_size - OVERHEAD;
        // Set the content of the block as free.
        __blkmngr_set_free(best_fitting, 0);
        // and! put a SIZE to the last four byte of the chunk
        char *block_ptr = (char *)best_fitting + real_size;
        if (remaining_size < (8 + OVERHEAD)) {
            goto no_split;
        } else if (remaining_size >= (8 + OVERHEAD)) {
            if (__blkmngr_is_free(stored_next)) {
                // Choice b)  merge!
                // Gather info about next block
                void *nextblock      = stored_next;
                block_t *n_nextblock = nextblock;
                // Remove next from list because it no longer exists(just unlink
                // it).
                __blkmngr_remove_from_freelist(header, n_nextblock);

                // Merge!
                block_t *merged_block = (block_t *)block_ptr;
                merged_block->size    = remaining_size + __blkmngr_get_real_size(n_nextblock->size);
                __blkmngr_set_free(merged_block, 1);

                merged_block->next = __blkmngr_get_next_block(header, stored_next);

                if (nextblock == header->tail) {
                    // I don't want to set it to tail now, instead, reclaim it
                    header->tail = merged_block;
                    // int reclaimSize = __blkmngr_get_real_size(t->size) + OVERHEAD;
                    // ksbrk(-reclaimSize);
                    // goto no_split;
                }
                // Then, add the merged block to the free list.
                __blkmngr_add_to_freelist(header, merged_block);
            } else {
                // Choice a)  seperate!
                block_t *putThisBack = (block_t *)block_ptr;
                putThisBack->size    = remaining_size - OVERHEAD;
                __blkmngr_set_free(putThisBack, 1);

                putThisBack->next = stored_next;

                if (base_ptr == header->tail) {
                    header->tail = putThisBack;
                    // int reclaimSize = __blkmngr_get_real_size(putThisBack->size) +OVERHEAD;
                    // ksbrk(-reclaimSize);
                    // goto no_split;
                }
                __blkmngr_add_to_freelist(header, putThisBack);
            }
            ((block_t *)base_ptr)->next = (block_t *)block_ptr;
        }
    no_split:
        // Remove the block from the free list.
        __blkmngr_remove_from_freelist(header, base_ptr);
        return (char *)base_ptr + sizeof(block_t);
    } else {
        block_t *ret;
        if (heap == &kernel_heap) {
            ret = ksbrk(block_size);
        } else {
            ret = usbrk(block_size);
        }
        assert(ret && "Heap is running out of space\n");
        if (!header->head) {
            header->head = ret;
        } else {
            header->tail->next = ret;
        }
        ret->next     = NULL;
        ret->nextfree = NULL;
        header->tail  = ret;
        void *save = ret;
        /* After sbrk(), split the block into half [block_size  | the rest],
         * and put the rest into the tree.
         */
        ret->size = block_size - OVERHEAD;
        __blkmngr_set_free(ret, 0);
        // Set the block allocated.
        // ptr = ptr + block_size - sizeof(uint32_t);
        // trailing_space = ptr;
        // *trailing_space = ret->size;
        // Now, return it!
        return (char *)save + sizeof(block_t);
    }
}

/// @brief Deallocates previously allocated space.
/// @param heap Heap to which we return the allocated memory.
/// @param ptr  Pointer to the allocated memory.
static void __do_free(vm_area_struct_t *heap, void *ptr)
{
    assert(ptr);

    // We will use these in writing.
    heap_header_t *header = (heap_header_t *)heap->vm_start;

    block_t *curr = (block_t *)((char *)ptr - sizeof(block_t));
    block_t *prev = __blkmngr_get_previous_block(header, curr);
    block_t *next = __blkmngr_get_next_block(header, curr);
    if (__blkmngr_is_free(prev) && __blkmngr_is_free(next)) {
        prev->size =
            __blkmngr_get_real_size(prev->size) + 2 * OVERHEAD +
            __blkmngr_get_real_size(curr->size) +
            __blkmngr_get_real_size(next->size);
        __blkmngr_set_free(prev, 1);

        prev->next = __blkmngr_get_next_block(header, next);

        // If next used to be tail, set prev = tail.
        if (header->tail == next) {
            header->tail = prev;
        }
        __blkmngr_remove_from_freelist(header, next);
    } else if (__blkmngr_is_free(prev)) {
        prev->size =
            __blkmngr_get_real_size(prev->size) + OVERHEAD + __blkmngr_get_real_size(curr->size);
        __blkmngr_set_free(prev, 1);

        prev->next = next;

        if (header->tail == curr) {
            header->tail = prev;
        }
    } else if (__blkmngr_is_free(next)) {
        // Change size to curr's size + OVERHEAD + next's size.
        curr->size =
            __blkmngr_get_real_size(curr->size) + OVERHEAD + __blkmngr_get_real_size(next->size);
        __blkmngr_set_free(curr, 1);

        curr->next = __blkmngr_get_next_block(header, next);

        if (header->tail == next) {
            header->tail = curr;
        }
        __blkmngr_remove_from_freelist(header, next);
        __blkmngr_add_to_freelist(header, curr);
    } else {
        // Just mark curr freed.
        __blkmngr_set_free(curr, 1);
        __blkmngr_add_to_freelist(header, curr);
    }
}

void kheap_init(size_t initial_size)
{
    unsigned int order = find_nearest_order_greater(0, initial_size);
    // Kernel_heap_start.
    kernel_heap.vm_start = __alloc_pages_lowmem(GFP_KERNEL, order);
    kernel_heap.vm_end   = kernel_heap.vm_start + ((1UL << order) * PAGE_SIZE);
    // Kernel_heap_start.
    kernel_heap_top = kernel_heap.vm_start;

    // FIXME!!
    // Set kernel_heap vm_area_struct info:
    //    kernel_heap.vm_next = NULL;
    //    kernel_heap.vm_mm = NULL;

    memset((void *)kernel_heap_top, 0, 3 * sizeof(block_t *));
    kernel_heap_top += 3 * sizeof(block_t *);
}

void *ksbrk(unsigned increment)
{
    return __do_brk(&kernel_heap_top, &kernel_heap, increment);
}

void *usbrk(unsigned increment)
{
    // Get the current process.
    task_struct *task = scheduler_get_current_process();
    assert(task && "There is no current task!\n");
    assert(task->mm && "The mm_struct of the current task is not initialized!\n");
    // Get the top address of the heap.
    uint32_t *heap_top = &task->mm->brk;
    // Get the heap.
    vm_area_struct_t *heap_segment = find_vm_area(task->mm, task->mm->start_brk);
    // Perform brk.
    return __do_brk(heap_top, heap_segment, increment);
}

void *sys_brk(void *addr)
{
    vm_area_struct_t *heap_segment;
    task_struct *task;
    mm_struct_t *mm;
    // Get the current process.
    task = scheduler_get_current_process();
    assert(task && "There is no current task!\n");
    assert(task->mm && "The mm_struct of the current task is not initialized!\n");
    // Get the heap.
    heap_segment = find_vm_area(task->mm, task->mm->start_brk);
    // Allocate the segment if don't exist.
    if (heap_segment == NULL) {
        heap_segment = create_vm_area(
            task->mm,
            0x40000000 /*FIXME! stabilize this*/,
            UHEAP_INITIAL_SIZE,
            MM_RW | MM_PRESENT | MM_USER | MM_UPDADDR,
            GFP_HIGHUSER);
        task->mm->start_brk = heap_segment->vm_start;
        task->mm->brk       = heap_segment->vm_start;
        // Reserved space for:
        // 1) First memory block.
        // static block_t *head = NULL;
        // 2) Last memory block.
        // static block_t *tail = NULL;
        // 3) All the memory blocks that are freed.
        // static block_t *freelist = NULL;
        task->mm->brk += 3 * sizeof(block_t *);
    }
    // If the address falls inside the memory region, call the free function,
    // otherwise execute a malloc of the specified amount.
    if (((uintptr_t)addr > heap_segment->vm_start) &&
        ((uintptr_t)addr < heap_segment->vm_end)) {
        __do_free(heap_segment, addr);
        return NULL;
    }
    return __do_malloc(heap_segment, (uintptr_t)addr);
}

void kheap_dump()
{
    // 1) First memory block.
    // static block_t *head = NULL;
    // 2) Last memory block.
    // static block_t *tail = NULL;
    // 3) All the memory blocks that are freed.
    // static block_t *freelist = NULL;

    // We will use these in writing.
    uint32_t *head     = (uint32_t *)(kernel_heap.vm_start);
    uint32_t *tail     = (uint32_t *)(kernel_heap.vm_start + sizeof(block_t *));
    uint32_t *freelist = (uint32_t *)(kernel_heap.vm_start + 2 * sizeof(block_t *));
    assert(head && tail && freelist && "Heap block lists point to null.");

    // We will use these others in reading.
    block_t *head_block = (block_t *)*head;
    // block_t *tail_block = (block_t *) *tail;
    block_t *first_free_block = (block_t *)*freelist;

    if (!head_block) {
        pr_debug("your heap is empty now\n");
        return;
    }

    // pr_debug("HEAP:\n");
    uint32_t total          = 0;
    uint32_t total_overhead = 0;
    block_t *it             = head_block;
    while (it) {
        pr_debug("[%c] %12u (%12u)   from 0x%p to 0x%p\n",
                 (__blkmngr_is_free(it)) ? 'F' : 'A',
                 __blkmngr_get_real_size(it->size),
                 it->size,
                 it,
                 (char *)it + OVERHEAD + __blkmngr_get_real_size(it->size));
        total += __blkmngr_get_real_size(it->size);
        total_overhead += OVERHEAD;
        it = it->next;
    }
    pr_debug("\nTotal usable bytes   : %d", total);
    pr_debug("\nTotal overhead bytes : %d", total_overhead);
    pr_debug("\nTotal bytes          : %d", total + total_overhead);
    pr_debug("\nFreelist: ");
    for (it = first_free_block; it != NULL; it = it->nextfree) {
        pr_debug("(%p)->", it);
    }
    pr_debug("\n\n");
    (void)total, (void)total_overhead;
}
