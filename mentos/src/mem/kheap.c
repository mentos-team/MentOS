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

/// @brief The size is encoded in the lowest 31 bits, while the 32nd is used to
/// determine if the block is free/used.
/// @param size the size of the block.
/// @return the real size of the block.
#define BLOCK_REAL_SIZE(size) ((size)&0x7FFFFFFF)

/// @brief Checks if the given block is actually free.
#define BLOCK_IS_FREE(block) ((block->size) & 0x80000000)

/// @brief Sets the block as free, by altering the bit of the size field.
#define BLOCK_SET_FREE(block) ((block->size) |= 0x80000000)

/// @brief Sets the block as used, by altering the bit of the size field.
#define BLOCK_SET_USED(block) ((block->size) &= 0x7FFFFFFF)

/// @brief Identifies a block of memory.
typedef struct block_t {
    /// @brief Identifies the size of the block and also if it is free or
    /// allocated.
    /// @details
    ///  |   1 bit    |      31 bit          |
    ///  | free/alloc | bits of real size    |
    ///  To calculate the real size, set to zero the last bit.
    unsigned int size;
    /// Entry in the list of blocks.
    list_head list;
    /// Entry in the list of free blocks.
    list_head free;
} block_t;

/// @brief Maps the heap memory to this three easily accessible values.
typedef struct {
    /// List of blocks.
    list_head list;
    /// List of free blocks.
    list_head free;
} heap_header_t;

/// @brief Returns the given size, rounded in multiples of 16.
/// @param size the given size.
/// @return the size rounded to the nearest multiple of 16.
static inline uint32_t __blkmngr_get_rounded_size(uint32_t size)
{
    return round_up(BLOCK_REAL_SIZE(size), 16);
}

/// @brief       Checks if the given size fits inside the block.
/// @param block The given block.
/// @param size  The size to check
/// @return
static inline int __blkmngr_does_it_fit(block_t *block, uint32_t size)
{
    assert(block && "Received null block.");
    return BLOCK_REAL_SIZE(block->size) >= BLOCK_REAL_SIZE(size);
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
                to_human_size(BLOCK_REAL_SIZE(block->size)),
                BLOCK_IS_FREE(block));
    } else {
        sprintf(buffer, "NULL");
    }
    return buffer;
}

static inline void __blkmngr_dump(heap_header_t *header)
{
#if __DEBUG_LEVEL__ == LOGLEVEL_DEBUG
    assert(header && "Received a NULL heap header.");
    pr_debug("\n");
    // Get the current process.
    task_struct *task = scheduler_get_current_process();
    assert(task && "There is no current task!\n");
    block_t *block;
    pr_debug("[%s] LIST (0x%p):\n", task->name, &header->list);
    list_for_each_decl(it, &header->list)
    {
        block = list_entry(it, block_t, list);
        pr_debug("[%s]     %s{", task->name, __block_to_string(block));
        if (it->prev != &header->list)
            pr_debug("0x%p", list_entry(it->prev, block_t, list));
        else
            pr_debug("   HEAD   ");
        pr_debug(", ");
        if (it->next != &header->list)
            pr_debug("0x%p", list_entry(it->next, block_t, list));
        else
            pr_debug("   HEAD   ");
        pr_debug("}\n");
    }
    pr_debug("[%s] FREE (0x%p):\n", task->name, &header->free);
    list_for_each_decl(it, &header->free)
    {
        block = list_entry(it, block_t, free);
        pr_debug("[%s]     %s{", task->name, __block_to_string(block));
        if (it->prev != &header->free)
            pr_debug("0x%p", list_entry(it->prev, block_t, free));
        else
            pr_debug("   HEAD   ");
        pr_debug(", ");
        if (it->next != &header->free)
            pr_debug("0x%p", list_entry(it->next, block_t, free));
        else
            pr_debug("   HEAD   ");
        pr_debug("}\n");
    }
    pr_debug("\n");
#endif
}

/// @brief Find the best fitting block in the memory pool.
/// @param header header describing the heap.
/// @param size the size we want.
/// @return a block that should fit our needs.
static inline block_t *__blkmngr_find_best_fitting(heap_header_t *header, uint32_t size)
{
    assert(header && "Received a NULL heap header.");
    block_t *best_fitting = NULL, *block;
    list_for_each_decl(it, &header->free)
    {
        block = list_entry(it, block_t, free);
        if (!BLOCK_IS_FREE(block))
            continue;
        if (!__blkmngr_does_it_fit(block, size))
            continue;
        if (!best_fitting || (BLOCK_REAL_SIZE(block->size) < BLOCK_REAL_SIZE(best_fitting->size)))
            best_fitting = block;
    }
    return best_fitting;
}

/// @brief Given a block, finds its previous block.
static inline block_t *__blkmngr_get_previous_block(heap_header_t *header, block_t *block)
{
    assert(header && "Received a NULL heap header.");
    assert(block && "Received null block.");
    // If the block is actually the head of the list, return NULL.
    if (block->list.prev == &header->list)
        return NULL;
    return list_entry(block->list.prev, block_t, list);
}

/// @brief Given a block, finds its next block.
static inline block_t *__blkmngr_get_next_block(heap_header_t *header, block_t *block)
{
    assert(header && "Received a NULL heap header.");
    assert(block && "Received null block.");
    // If the block is actually the tail of the list, return NULL.
    if (block->list.next == &header->list)
        return NULL;
    return list_entry(block->list.next, block_t, list);
}

static inline void __blkmngr_split_block(heap_header_t *header, block_t *block, uint32_t size)
{
    assert(block && "Received NULL block.");
    assert(BLOCK_IS_FREE(block) && "The block is not free.");

    pr_debug("Splitting %s\n", __block_to_string(block));

    // Create the new block.
    block_t *split = (block_t *)((char *)block + OVERHEAD + BLOCK_REAL_SIZE(size));

    // Insert the block in the list.
    list_head_insert_after(&split->list, &block->list);
    // Insert the block in the free list.
    list_head_insert_after(&split->free, &block->free);
    // Update the size of the new block.
    split->size = BLOCK_REAL_SIZE(block->size) - OVERHEAD - size;
    // Update the size of the base block.
    block->size = BLOCK_REAL_SIZE(size);
    // Set the blocks as free.
    BLOCK_SET_FREE(block);
    BLOCK_SET_FREE(split);

    pr_debug("Into      %s\n", __block_to_string(block));
    pr_debug("And       %s\n", __block_to_string(split));
}

static inline void __blkmngr_merge_blocks(heap_header_t *header, block_t *block, block_t *other)
{
    assert(block && "Received NULL first block.");
    assert(other && "Received NULL second block.");
    assert(BLOCK_IS_FREE(block) && "The first block is not free.");
    assert(BLOCK_IS_FREE(other) && "The second block is not free.");
    if (block->list.next != &other->list) {
        __blkmngr_dump(header);
        pr_err("Cannot merge %s", __block_to_string(block));
        pr_err(" and %s, they are not adjacent!\n", __block_to_string(other));
        assert(block->list.next == &other->list && "The blocks are not adjacent.");
    }
    pr_debug("Merging %s\n", __block_to_string(block));
    pr_debug("And     %s\n", __block_to_string(other));

    // Remove the block from the free list.
    list_head_remove(&other->free);
    // Remove the block from the list.
    list_head_remove(&other->list);
    // Update the size.
    block->size = BLOCK_REAL_SIZE(block->size) + BLOCK_REAL_SIZE(other->size) + OVERHEAD;
    // Set the splitted block as free.
    BLOCK_SET_FREE(block);

    pr_debug("Into    %s\n", __block_to_string(block));
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
    pr_notice("Expanding heap from 0x%p to 0x%p.\n", mm->brk, new_heap_top);
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
    // Calculate size that's used, round it to multiple of 16.
    uint32_t rounded_size = __blkmngr_get_rounded_size(size);
    // Calculate actual size we need, which is the rounded size
    uint32_t actual_size = rounded_size + OVERHEAD;
    pr_debug("Searching block of size: %s\n", to_human_size(rounded_size));
    // Find the best fitting block.
    block_t *block = __blkmngr_find_best_fitting(header, rounded_size);
    // If we were able to find a suitable block, either split it, or return it.
    if (block) {
        if (BLOCK_REAL_SIZE(block->size) > actual_size) {
            // Split the block, provide the rounded size to the function.
            __blkmngr_split_block(header, block, rounded_size);
        } else {
            pr_debug("Found perfect block: %s\n", __block_to_string(block));
        }
        // Remove the block from the free list.
        list_head_remove(&block->free);
    } else {
        pr_debug("Failed to find suitable block, we need to create a new one.\n");
        // We need more space, specifically the size of the block plus the size
        // of the block_t structure.
        block = __do_brk(heap, actual_size);
        // Set the size.
        block->size = rounded_size;
        // Check the block.
        assert(block && "Failed to create a new block!");
        // Setup the new block.
        list_head_init(&block->list);
        list_head_init(&block->free);
        list_head_insert_before(&block->list, &header->list);
    }
    // Set the new block as used.
    BLOCK_SET_USED(block);
    __blkmngr_dump(header);
    return (void *)((char *)block + OVERHEAD);
}

/// @brief Deallocates previously allocated space.
/// @param heap Heap to which we return the allocated memory.
/// @param ptr  Pointer to the allocated memory.
static void __do_free(vm_area_struct_t *heap, void *ptr)
{
    // We will use these in writing.
    heap_header_t *header = (heap_header_t *)heap->vm_start;
    // Get the current block.
    block_t *block = (block_t *)((char *)ptr - OVERHEAD);
    // Get the previous block.
    block_t *prev = __blkmngr_get_previous_block(header, block);
    // Get the next block.
    block_t *next = __blkmngr_get_next_block(header, block);
    pr_debug("Freeing block %s\n", __block_to_string(block));
    // Set the block free.
    BLOCK_SET_FREE(block);
    // Merge adjacent blocks.
    if (prev && BLOCK_IS_FREE(prev) && next && BLOCK_IS_FREE(next)) {
        pr_debug("Merging with previous and next.\n");
        __blkmngr_merge_blocks(header, prev, block);
        __blkmngr_merge_blocks(header, prev, next);
    } else if (prev && BLOCK_IS_FREE(prev)) {
        pr_debug("Merging with previous.\n");
        __blkmngr_merge_blocks(header, prev, block);
    } else if (next && BLOCK_IS_FREE(next)) {
        pr_debug("Merging with next.\n");
        // Merge the blocks.
        __blkmngr_merge_blocks(header, block, next);
        // Add the block to the free list.
        list_head_insert_before(&block->free, &header->free);
    } else {
        pr_debug("No merging required.\n");
        // Add the block to the free list.
        list_head_insert_before(&block->free, &header->free);
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
        pr_debug("Heap start : 0x%p.\n", heap->vm_start);
        pr_debug("Heap end   : 0x%p.\n", heap->vm_end);
        // Initialize the memory.
        memset((char *)heap->vm_start, 0, segment_size);
        // Save where the original heap starts.
        task->mm->start_brk = heap->vm_start;
        // Initialize the header.
        heap_header_t *header = (heap_header_t *)heap->vm_start;
        list_head_init(&header->list);
        list_head_init(&header->free);
        // Preare the first block.
        block_t *block = (block_t *)(header + sizeof(heap_header_t));
        // Let us start with a block of 1 Kb.
        block->size = K;
        list_head_init(&block->list);
        list_head_init(&block->free);
        // Insert the block inside the heap.
        list_head_insert_before(&block->list, &header->list);
        list_head_insert_before(&block->free, &header->free);
        // Save where the heap actually start.
        task->mm->brk = (uint32_t)((char *)block + OVERHEAD + BLOCK_REAL_SIZE(block->size));
        // Set the block as free.
        BLOCK_SET_FREE(block);
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
