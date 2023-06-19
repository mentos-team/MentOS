/// @file kheap.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "kernel.h"
#include "stddef.h"
#include "sys/kernel_levels.h"         // Include kernel log levels.
#define __DEBUG_HEADER__ "[KHEAP ]"    ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_INFO ///< Set log level.
#include "io/debug.h"                  // Include debugging functions.

#include "mem/kheap.h"
#include "mem/paging.h"
#include "sys/list_head.h"
#include "sys/bitops.h"
#include "stdbool.h"
#include "stdint.h"
#include "string.h"
#include "assert.h"
#include "stdio.h"
#include "math.h"

/// Overhead given by the block_t itself.
#define OVERHEAD sizeof(block_t)
/// Align the given address.
#define ADDR_ALIGN(addr) ((((uint32_t)(addr)) & 0xFFFFF000) + 0x1000)
/// Checks if the given address is aligned.
#define IS_ALIGN(addr) ((((uint32_t)(addr)) & 0x00000FFF) == 0)

/// @brief Identifies a block of memory.
typedef struct block_t {
    /// @brief Identifies the side of the block and also if it is free or allocated.
    /// @details
    ///  |            31 bit             |   1 bit    |
    ///  |    first bits of real size    | free/alloc |
    ///  To calculate the real size, set to zero the last bit
    uint32_t size;
    /// @brief Magic number to check if the memory is corrupted.
    uint32_t magic;
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
    return round_up(__blkmngr_get_real_size(size), 16);
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
    return (__blkmngr_get_real_size(block->size) >= __blkmngr_get_real_size(size)) && __blkmngr_is_free(block);
}

/// @brief Prepares a string that represents the block.
/// @param block the block to represent.
/// @return a string with the block info.
static inline const char *__block_to_string(block_t *block)
{
    static char buffer[256];
    if (block) {
        sprintf(buffer, "0x%p [%9s](%d)",
                block,
                to_human_size(__blkmngr_get_real_size(block->size)),
                __blkmngr_is_free(block));
    } else {
        sprintf(buffer, "NULL");
    }
    return buffer;
}

static inline void __blkmngr_dump(heap_header_t *header)
{
    assert(header && "Received a NULL heap header.");
    pr_debug("\n");
    if (header->head) {
        pr_debug("# LIST:\n");
        for (block_t *it = header->head; it; it = it->next) {
            pr_debug("#     %s", __block_to_string(it));
            pr_debug("{next: %s,", __block_to_string(it->next));
            pr_debug(" nextfree: %s}\n", __block_to_string(it->nextfree));
        }
        pr_debug("\n");
    }
    if (header->free) {
        pr_debug("# FREE:\n");
        for (block_t *it = header->free; it; it = it->nextfree) {
            pr_debug("#     %s", __block_to_string(it));
            pr_debug("{next: %s,", __block_to_string(it->next));
            pr_debug(" nextfree: %s}\n", __block_to_string(it->nextfree));
        }
    }
    pr_debug("\n");
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
        if ((best_fitting == NULL) ||
            (__blkmngr_get_real_size(current->size) < __blkmngr_get_real_size(best_fitting->size))) {
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

/// @brief Given a block, finds its last block.
static inline block_t *__blkmngr_get_last_block(heap_header_t *header)
{
    assert(header && "Received a NULL heap header.");
    block_t *block = header->head;
    while (block && block->next) block = block->next;
    return block;
}

static inline void __blkmngr_split_block(heap_header_t *header, block_t *block, uint32_t size)
{
    assert(block && "Received NULL block.");
    assert(__blkmngr_is_free(block) && "The block is not free.");
    pr_debug("Splitting %s", __block_to_string(block));
    pr_debug("{next: %s,", __block_to_string(block->next));
    pr_debug(" nextfree: %s}\n", __block_to_string(block->nextfree));
    // Create the new block.
    block_t *split = (block_t *)((char *)block + OVERHEAD + size);
    // Update the pointer of the new block.
    split->next     = block->next;
    split->nextfree = block->nextfree;
    // Update the pointer of the base block.
    block->next     = split;
    block->nextfree = split;
    // Update the size of the new block.
    split->size = __blkmngr_get_real_size(block->size) - OVERHEAD - size;
    // Update the size of the base block.
    block->size = __blkmngr_get_real_size(size);
    // Set the splitted block as free.
    __blkmngr_set_free(split, 1);
    pr_debug("Into %s", __block_to_string(block));
    pr_debug("{next: %s,", __block_to_string(block->next));
    pr_debug(" nextfree: %s}\n", __block_to_string(block->nextfree));
    pr_debug("And %s", __block_to_string(split));
    pr_debug("{next: %s,", __block_to_string(split->next));
    pr_debug(" nextfree: %s}\n", __block_to_string(split->nextfree));
}

static inline void __blkmngr_merge_blocks(heap_header_t *header, block_t *block1, block_t *block2)
{
    assert(block1 && "Received NULL first block.");
    assert(block2 && "Received NULL second block.");
    assert(__blkmngr_is_free(block1) && "The first block is not free.");
    assert(__blkmngr_is_free(block2) && "The second block is not free.");
    assert(block1->next == block2 && "The blocks are not adjacent.");

    pr_debug("Merging %s", __block_to_string(block1));
    pr_debug("{next: %s,", __block_to_string(block1->next));
    pr_debug(" nextfree: %s}\n", __block_to_string(block1->nextfree));
    pr_debug("And %s", __block_to_string(block2));
    pr_debug("{next: %s,", __block_to_string(block2->next));
    pr_debug(" nextfree: %s}\n", __block_to_string(block2->nextfree));

    // Remove the block from the free list.
    __blkmngr_remove_from_freelist(header, block2);
    // Merge the block.
    block1->next = block2->next;
    // Point to the next free block.
    // block1->nextfree = block2->nextfree;
    // Update the size.
    block1->size = __blkmngr_get_real_size(block1->size) +
                   __blkmngr_get_real_size(block2->size) + OVERHEAD;
    // Set the splitted block as free.
    __blkmngr_set_free(block1, 1);

    pr_debug("Into %s", __block_to_string(block1));
    pr_debug("{next: %s,", __block_to_string(block1->next));
    pr_debug(" nextfree: %s}\n", __block_to_string(block1->nextfree));
}

/// @brief Extends the provided heap of the given increment.
/// @param heap      Pointer to the heap.
/// @param increment Increment to the heap.
/// @return Pointer to the old top of the heap, ready to be used.
static void *__do_brk(vm_area_struct_t *heap, uint32_t increment)
{
    assert(heap && "Pointer to the heap is NULL.");
    // Get the current process.
    task_struct *task = scheduler_get_current_process();
    assert(task && "There is no current task!\n");
    // Get the memory descriptor.
    mm_struct_t *mm = task->mm;
    assert(mm && "The mm_struct of the current task is not initialized!\n");
    // Compute the new heap top.
    uint32_t new_heap_top = mm->brk + increment;
    // Debugging message.
    pr_notice("Expanding heap from %p to %p.\n", mm->brk, new_heap_top);
    // If new boundary is larger than the end, we fail.
    if (new_heap_top > heap->vm_end) {
        pr_err("The new boundary is larger than the end!");
        return NULL;
    }
    // Overwrite the top of the heap.
    mm->brk = new_heap_top;
    // Return the old top of the heap.
    return (void *)(mm->brk - increment);
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
    pr_debug("Searching block of size: %s\n", to_human_size(rounded_size));
    // Find the best fitting block.
    block_t *block = __blkmngr_find_best_fitting(header, rounded_size);
    if (block) {
        if (__blkmngr_get_real_size(block->size) > rounded_size) {
            // Split the block.
            __blkmngr_split_block(header, block, rounded_size);
        } else {
            pr_debug("Found perferct block: %s\n", __block_to_string(block));
        }
        // Set the block as used.
        __blkmngr_set_free(block, 0);
        // Remove the block from the free list.
        __blkmngr_remove_from_freelist(header, block);
    } else {
        pr_debug("Failed to find suitable block, we need to create a new one.\n");
        // We need more space, specifically the size of the block plus the size
        // of the block_t structure.
        block = __do_brk(heap, rounded_size + OVERHEAD);
        // Check the block.
        assert(block && "Failed to create a new block!");
        // Get the last block in the list.
        block_t *last = __blkmngr_get_last_block(header);
        assert(last && "Cannot find the last block of the list!");
        // Add the new block to the list.
        last->next = last;
        // Set the new block as used.
        __blkmngr_set_free(block, 0);
    }
    __blkmngr_dump(header);
    return (void *)((uintptr_t)block + OVERHEAD);
}

/// @brief Deallocates previously allocated space.
/// @param heap Heap to which we return the allocated memory.
/// @param ptr  Pointer to the allocated memory.
static void __do_free(vm_area_struct_t *heap, void *ptr)
{
    // We will use these in writing.
    heap_header_t *header = (heap_header_t *)heap->vm_start;
    // Get the current block.
    block_t *block = (block_t *)((uintptr_t)ptr - OVERHEAD);
    // Get the previous block.
    block_t *prev = __blkmngr_get_previous_block(header, block);
    // Get the next block.
    block_t *next = __blkmngr_get_next_block(header, block);
    // Set the block free.
    __blkmngr_set_free(block, 1);
    pr_debug("Freeing block %s\n", __block_to_string(block));
    // Merge adjacent blocks.
    if (__blkmngr_is_free(prev) && __blkmngr_is_free(next)) {
        __blkmngr_merge_blocks(header, prev, block);
        __blkmngr_merge_blocks(header, prev, next);
    } else if (__blkmngr_is_free(prev)) {
        __blkmngr_merge_blocks(header, prev, block);
    } else if (__blkmngr_is_free(next)) {
        // Merge the blocks.
        __blkmngr_merge_blocks(header, block, next);
        // Add the block to the free list.
        __blkmngr_add_to_freelist(header, block);
    } else {
        // Add the block to the free list.
        __blkmngr_add_to_freelist(header, block);
    }
    __blkmngr_dump(header);
}

void *sys_brk(void *addr)
{
    // Get the current process.
    task_struct *task = scheduler_get_current_process();
    assert(task && "There is no current task!\n");
    // Get the memory descriptor.
    mm_struct_t *mm = task->mm;
    assert(mm && "The mm_struct of the current task is not initialized!\n");
    // Get the heap.
    vm_area_struct_t *heap = find_vm_area(task->mm, task->mm->start_brk);
    // Allocate the segment if don't exist.
    if (heap == NULL) {
        pr_debug("Allocating heap!\n");
        // Set the heap to 4 Mb.
        size_t heap_size = 4 * M;
        // Add to that the space required to store the header, and the first block.
        size_t segment_size = heap_size + sizeof(heap_header_t) + sizeof(block_t);
        // TODO: Randomize VM start, and check why it is 0x40000000 and not
        // 0x400000.
        heap = create_vm_area(
            task->mm,
            0x40000000,
            segment_size,
            MM_RW | MM_PRESENT | MM_USER | MM_UPDADDR,
            GFP_HIGHUSER);
        pr_debug("Heap size  : %s.\n", to_human_size(heap_size));
        pr_debug("Heap start : %p.\n", heap->vm_start);
        pr_debug("Heap end   : %p.\n", heap->vm_end);
        // Initialize the memory.
        memset((char *)heap->vm_start, 0, segment_size);
        // Save where the original heap starts.
        task->mm->start_brk = heap->vm_start;
        // Save where the heap actually start.
        task->mm->brk = heap->vm_start + sizeof(heap_header_t);
        // Initialize the header.
        heap_header_t *header = (heap_header_t *)heap->vm_start;
        header->head          = (block_t *)(header + sizeof(heap_header_t));
        header->free          = header->head;
        // Preare the first block.
        block_t *first  = header->head;
        first->size     = heap_size;
        first->next     = NULL;
        first->nextfree = NULL;
        // Set the block as free.
        __blkmngr_set_free(first, 1);
        __blkmngr_dump(header);
    }
    void *_ret = NULL;
    // If the address falls inside the memory region, call the free function,
    // otherwise execute a malloc of the specified amount.
    if (((uintptr_t)addr > heap->vm_start) && ((uintptr_t)addr < heap->vm_end)) {
        __do_free(heap, addr);
    } else {
        _ret = __do_malloc(heap, (size_t)addr);
    }
    return _ret;
}
