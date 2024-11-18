/// @file kheap.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"         // Include kernel log levels.
#define __DEBUG_HEADER__ "[KHEAP ]"    ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_INFO ///< Set log level.
#include "io/debug.h"                  // Include debugging functions.

#include "assert.h"
#include "kernel.h"
#include "math.h"
#include "mem/kheap.h"
#include "mem/paging.h"
#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/bitops.h"
#include "list_head.h"

/// @brief The size of the heap in bytes, defined as 4 megabytes.
#define HEAP_SIZE (4 * M)

/// @brief The lower bound address for virtual memory area placement.
/// This address marks the starting point of the heap.
#define HEAP_VM_LB 0x40000000

/// @brief The upper bound address for virtual memory area placement.
/// This address marks the endpoint of the heap, ensuring no overlap with other memory regions.
#define HEAP_VM_UB 0x50000000

/// @brief The overhead introduced by the block_t structure itself.
/// This is used for memory management and bookkeeping.
#define OVERHEAD sizeof(block_t)

/// @brief Aligns the given address to the nearest upper page boundary.
/// The address will be aligned to 4096 bytes (0x1000).
/// @param addr The address to align.
/// @return The aligned address.
#define ADDR_ALIGN(addr) ((((uint32_t)(addr)) & 0xFFFFF000) + 0x1000)

/// @brief Checks if the given address is aligned to a page boundary.
/// @param addr The address to check.
/// @return Non-zero value if the address is aligned, zero otherwise.
#define IS_ALIGN(addr) ((((uint32_t)(addr)) & 0x00000FFF) == 0)

/// @brief Represents a block of memory within the heap.
/// This structure includes metadata for managing memory allocation and free status.
typedef struct block_t {
    /// @brief A single bit indicating if the block is free (1) or allocated (0).
    unsigned int is_free : 1;

    /// @brief The size of the block in bytes.
    /// This includes the space for the block's overhead.
    unsigned int size : 31;

    /// @brief Entry in the list of all blocks in the heap.
    list_head list;

    /// @brief Entry in the list of free blocks.
    list_head free;
} block_t;

/// @brief Maps the heap memory to easily accessible values.
/// This structure contains pointers to the list of all blocks and the list of free blocks.
typedef struct {
    /// @brief List of all memory blocks, both free and allocated.
    list_head list;

    /// @brief List of free blocks available for allocation.
    list_head free;
} heap_header_t;

/// @brief Returns the given size, rounded to the nearest multiple of 16. This
/// is useful for ensuring memory alignment requirements are met.
/// @param size The given size to be rounded.
/// @return The size rounded to the nearest multiple of 16.
static inline uint32_t __blkmngr_get_rounded_size(uint32_t size)
{
    return round_up(size, 16);
}

/// @brief Checks if the given size fits inside the block. This function
/// verifies whether the specified size can be accommodated within the block's
/// available size.
/// @param block The given block to check. Must not be NULL.
/// @param size  The size to check against the block's size.
/// @return 1 if the size fits within the block, -1 if the block is NULL.
static inline int __blkmngr_does_it_fit(block_t *block, uint32_t size)
{
    // Check for a null block pointer to avoid dereferencing a null pointer
    if (!block) {
        pr_crit("Received null block.\n");
        return -1; // Error: Block pointer is NULL.
    }

    // Check if the block can accommodate the requested size.
    return block->size >= size;
}

/// @brief Prepares a string that represents the block. This function formats
/// the information of the specified block into a human-readable string.
/// @param block The block to represent. Can be NULL.
/// @return A string containing the block's address, size, and free status.
static inline const char *__block_to_string(block_t *block)
{
    // Static buffer to hold the string representation of the block.
    static char buffer[256];

    if (block) {
        // Format the block's information into the buffer
        sprintf(buffer, "0x%p [%9s](%d)",
                (void *)block,              // Pointer to the block
                to_human_size(block->size), // Human-readable size
                block->is_free);            // Free status (1 if free, 0 if allocated)
    } else {
        // If the block is NULL, indicate this in the buffer.
        sprintf(buffer, "NULL");
    }

    return buffer; // Return the formatted string.
}

/// @brief Dumpts debug information about the heap.
/// @param header the heap header.
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
    // Check if the header is NULL, log an error and return NULL
    if (!header) {
        pr_crit("Received a NULL heap header.\n");
        return NULL; // Return NULL to indicate failure.
    }

    block_t *best_fitting = NULL; // Initialize the best fitting block to NULL.
    block_t *block;               // Declare a pointer for the current block.

    // Iterate over the list of free blocks.
    list_for_each_decl(it, &header->free)
    {
        // Get the current block from the list.
        block = list_entry(it, block_t, free);

        // Skip if the block is not free.
        if (!block->is_free) {
            continue;
        }

        // Check if the block can accommodate the requested size.
        if (!__blkmngr_does_it_fit(block, size)) {
            continue;
        }

        // Update the best fitting block if it's the first found or smaller than
        // the current best.
        if (!best_fitting || (block->size < best_fitting->size)) {
            best_fitting = block;
        }
    }

    // Return the best fitting block found, or NULL if none were suitable.
    return best_fitting;
}

/// @brief Given a block, finds its previous block.
/// @param header the heap header.
/// @param block the block.
/// @return a pointer to the previous block or NULL if an error occurs.
static inline block_t *__blkmngr_get_previous_block(heap_header_t *header, block_t *block)
{
    // Check if the heap header is valid.
    if (!header) {
        pr_crit("Received a NULL heap header.\n");
        return NULL; // Return NULL to indicate failure.
    }

    // Check if the block is valid.
    if (!block) {
        pr_crit("Received a NULL block.\n");
        return NULL; // Return NULL to indicate failure.
    }

    // If the block is the head of the list, return NULL.
    if (block->list.prev == &header->list) {
        return NULL; // No previous block exists.
    }

    // Return the previous block by accessing the list entry.
    return list_entry(block->list.prev, block_t, list);
}

/// @brief Given a block, finds its next block in the memory pool.
/// @param header The heap header containing information about the heap.
/// @param block The current block for which the next block is to be found.
/// @return A pointer to the next block if it exists, or NULL if an error occurs or if the block is the last one.
static inline block_t *__blkmngr_get_next_block(heap_header_t *header, block_t *block)
{
    // Check if the heap header is valid.
    if (!header) {
        pr_crit("Received a NULL heap header.\n");
        return NULL; // Return NULL to indicate failure.
    }

    // Check if the block is valid.
    if (!block) {
        pr_crit("Received a NULL block.\n");
        return NULL; // Return NULL to indicate failure.
    }

    // If the block is the tail of the list, return NULL as there is no next block.
    if (block->list.next == &header->list) {
        return NULL; // No next block exists.
    }

    // Return the next block by accessing the list entry.
    return list_entry(block->list.next, block_t, list);
}

/// @brief Checks if the given `previous` block is actually the block that comes
/// before `block` in the memory pool.
/// @param block The current block from which we are checking the previous block.
/// @param previous The block that is supposedly the previous block.
/// @return 1 if `previous` is the actual previous block of `block`, 0
/// otherwise. Returns -1 on error.
static inline int __blkmngr_is_previous_block(block_t *block, block_t *previous)
{
    // Check if the current block is valid.
    if (!block) {
        pr_crit("Received a NULL block.\n");
        return -1; // Error: Invalid block.
    }

    // Check if the previous block is valid.
    if (!previous) {
        pr_crit("Received a NULL previous block.\n");
        return -1; // Error: Invalid previous block.
    }

    // Check if the previous block's list entry matches the previous entry of
    // the current block.
    return block->list.prev == &previous->list;
}

/// @brief Checks if the given `next` block is actually the block that comes
/// after `block` in the memory pool.
/// @param block The current block from which we are checking the next block.
/// @param next The block that is supposedly the next block.
/// @return 1 if `next` is the actual next block of `block`, 0 otherwise.
/// Returns -1 on error.
static inline int __blkmngr_is_next_block(block_t *block, block_t *next)
{
    // Check if the current block is valid.
    if (!block) {
        pr_crit("Received a NULL block.\n");
        return -1; // Error: Invalid block.
    }

    // Check if the next block is valid.
    if (!next) {
        pr_crit("Received a NULL next block.\n");
        return -1; // Error: Invalid next block.
    }

    // Check if the next block's list entry matches the next entry of the
    // current block.
    return block->list.next == &next->list;
}

/// @brief Splits a block into two blocks based on the specified size for the first block.
/// @param header The heap header containing metadata about the memory pool.
/// @param block The block to be split, which must be free.
/// @param size The size of the first of the two new blocks. Must be less than the original block's size minus overhead.
/// @return 0 on success, -1 on error.
static inline int __blkmngr_split_block(heap_header_t *header, block_t *block, uint32_t size)
{
    // Check if the block is valid.
    if (!block) {
        pr_crit("Received NULL block.\n");
        return -1; // Cannot proceed without a valid block.
    }

    // Check if the block is free; splitting a used block is not allowed.
    if (!block->is_free) {
        pr_crit("The block is not free and cannot be split.\n");
        return -1; // Cannot split a non-free block.
    }

    // Check if the requested size is valid (greater than 0 and less than the
    // current block size minus overhead).
    if ((size == 0) || (size + OVERHEAD >= block->size)) {
        pr_crit("Invalid size for splitting: size must be > 0 and < %u.\n", block->size - OVERHEAD);
        return -1; // Size is invalid for splitting.
    }

    pr_debug("Splitting %s\n", __block_to_string(block));

    // Create the new block by calculating its address based on the original block and the specified size.
    block_t *split = (block_t *)((char *)block + OVERHEAD + size);

    // Insert the new block into the main list after the current block.
    list_head_insert_after(&split->list, &block->list);

    // Insert the new block into the free list.
    list_head_insert_after(&split->free, &block->free);

    // Update the size of the new block to reflect the remaining size after splitting.
    split->size = block->size - OVERHEAD - size;

    // Update the size of the original block to the new size.
    block->size = size;

    // Mark both blocks as free.
    block->is_free = 1;
    split->is_free = 1;

    pr_debug("Into      %s\n", __block_to_string(block));
    pr_debug("And       %s\n", __block_to_string(split));

    return 0; // Success
}

/// @brief Merges two adjacent blocks into the first block, effectively expanding its size.
/// @param header The heap header containing metadata about the memory pool.
/// @param block The first block that will be expanded.
/// @param other The second block that will be merged into the first block, becoming invalid in the process.
/// @return 0 on success, -1 on error.
static inline int __blkmngr_merge_blocks(heap_header_t *header, block_t *block, block_t *other)
{
    // Check if the first block is valid.
    if (!block) {
        pr_crit("Received NULL first block.\n");
        return -1; // Cannot proceed without a valid first block.
    }

    // Check if the second block is valid.
    if (!other) {
        pr_crit("Received NULL second block.\n");
        return -1; // Cannot proceed without a valid second block.
    }

    // Check if the first block is free.
    if (!block->is_free) {
        pr_crit("The first block is not free.\n");
        return -1; // Cannot merge a non-free block.
    }

    // Check if the second block is free.
    if (!other->is_free) {
        pr_crit("The second block is not free.\n");
        return -1; // Cannot merge with a non-free block.
    }

    // Check if the blocks are adjacent.
    if (!__blkmngr_is_next_block(block, other)) {
        pr_crit("The blocks are not adjacent and cannot be merged.\n");
        return -1; // Blocks must be adjacent to merge.
    }

    pr_debug("Merging %s\n", __block_to_string(block));
    pr_debug("And     %s\n", __block_to_string(other));

    // Remove the other block from the free list, effectively marking it as not available.
    list_head_remove(&other->free);

    // Remove the other block from the main list.
    list_head_remove(&other->list);

    // Update the size of the first block to include the size of the second block and overhead.
    block->size += other->size + OVERHEAD;

    // The first block remains free after merging, so set its free status.
    block->is_free = 1;

    pr_debug("Into    %s\n", __block_to_string(block));

    return 0; // Success
}

/// @brief Extends the provided heap by a specified increment.
/// @param heap Pointer to the heap structure that tracks the heap's memory area.
/// @param increment The amount by which to increase the heap size.
/// @return Pointer to the old top of the heap, or NULL if an error occurred.
static void *__do_brk(vm_area_struct_t *heap, uint32_t increment)
{
    // Check if the heap pointer is valid.
    if (!heap) {
        pr_crit("Pointer to the heap is NULL.\n");
        return NULL; // Cannot proceed without a valid heap pointer.
    }

    // Get the current process.
    task_struct *task = scheduler_get_current_process();
    // Check if there is a current task.
    if (!task) {
        pr_crit("There is no current task!\n");
        return NULL; // Cannot proceed without a current task.
    }

    // Get the memory descriptor for the current task.
    mm_struct_t *mm = task->mm;
    // Check if the memory descriptor is initialized.
    if (!mm) {
        pr_crit("The mm_struct of the current task is not initialized!\n");
        return NULL; // Cannot proceed without a valid mm_struct.
    }

    // Compute the new top of the heap by adding the increment to the current break.
    uint32_t new_heap_top = mm->brk + increment;

    // Debugging message to indicate the expansion of the heap.
    pr_debug("Expanding heap from 0x%p to 0x%p.\n", mm->brk, new_heap_top);

    // Check if the new heap top exceeds the boundaries of the heap.
    if (new_heap_top > heap->vm_end) {
        pr_err("The new boundary is larger than the end of the heap!\n");
        return NULL; // New boundary exceeds the allowed heap area.
    }

    // Update the break (top of the heap) to the new value.
    mm->brk = new_heap_top;

    // Return the pointer to the old top of the heap before the increment.
    return (void *)(mm->brk - increment);
}

/// @brief Allocates a specified number of bytes of uninitialized storage from the heap.
/// @param heap Pointer to the heap from which we will allocate memory.
/// @param size The size of the desired memory area to allocate.
/// @return Pointer to the allocated memory area, or NULL if allocation fails.
static void *__do_malloc(vm_area_struct_t *heap, size_t size)
{
    // Check if the heap pointer is valid.
    if (!heap) {
        pr_crit("Pointer to the heap is NULL.\n");
        return NULL; // Cannot proceed without a valid heap pointer.
    }

    // Check if the requested size is zero.
    if (size == 0) {
        pr_err("Requested allocation size is zero.\n");
        return NULL; // No allocation for zero size.
    }

    // Get the heap header to access the memory management structures.
    heap_header_t *header = (heap_header_t *)heap->vm_start;
    if (!header) {
        pr_err("Heap header is NULL.\n");
        return NULL; // Cannot proceed without a valid heap header.
    }

    // Calculate the size that is used, rounding it to the nearest multiple of 16.
    uint32_t rounded_size = __blkmngr_get_rounded_size(size);
    // Calculate the actual size required, which includes overhead for the block header.
    uint32_t actual_size = rounded_size + OVERHEAD;

    pr_debug("Searching for a block of size: %s\n", to_human_size(rounded_size));

    // Find the best fitting block in the heap.
    block_t *block = __blkmngr_find_best_fitting(header, rounded_size);

    // If a suitable block is found, either split it or use it directly.
    if (block) {
        // Check if the block size is larger than the actual size needed.
        if (block->size > actual_size) {
            // Split the block, providing the rounded size to the splitting function.
            if (__blkmngr_split_block(header, block, rounded_size) < 0) {
                pr_err("Failed to split the block.\n");
                return NULL; // Return NULL if splitting fails.
            }
        } else {
            pr_debug("Found perfect block: %s\n", __block_to_string(block));
        }

        // Remove the block from the free list to mark it as allocated.
        list_head_remove(&block->free);
    } else {
        pr_debug("Failed to find a suitable block; creating a new one.\n");

        // We need more space, specifically the size of the block plus the size of the block_t structure.
        block = __do_brk(heap, actual_size);
        // Check if the block allocation was successful.
        if (!block) {
            pr_err("Failed to create a new block.\n");
            return NULL; // Return NULL if block creation fails.
        }

        // Set the size for the newly created block.
        block->size = rounded_size;

        // Initialize the new block's list pointers.
        list_head_init(&block->list);
        list_head_init(&block->free);
        // Insert the block into the header's list of blocks.
        list_head_insert_before(&block->list, &header->list);
    }

    // Set the block as used (allocated).
    block->is_free = 0;

    // Optionally dump the current state of the heap for debugging.
    __blkmngr_dump(header);

    // Return a pointer to the memory area, skipping the block header.
    return (void *)((char *)block + OVERHEAD);
}

/// @brief Deallocates previously allocated space, returning it to the heap.
/// @param heap Pointer to the heap to which the allocated memory is returned.
/// @param ptr Pointer to the previously allocated memory to be deallocated.
/// @return 0 on success, -1 on error.
static int __do_free(vm_area_struct_t *heap, void *ptr)
{
    // Check if the heap pointer is valid.
    if (!heap) {
        pr_crit("Pointer to the heap is NULL.\n");
        return -1; // Return error if the heap pointer is NULL.
    }

    // Check if the pointer to be freed is NULL.
    if (!ptr) {
        pr_err("Attempted to free a NULL pointer.\n");
        return -1; // Return error if the pointer is NULL.
    }

    // Get the heap header to access the memory management structures.
    heap_header_t *header = (heap_header_t *)heap->vm_start;
    if (!header) {
        pr_err("Heap header is NULL.\n");
        return -1; // Return error if the heap header is not valid.
    }

    // Calculate the block pointer from the provided pointer.
    block_t *block = (block_t *)((char *)ptr - OVERHEAD);
    if (!block) {
        pr_err("Calculated block pointer is NULL.\n");
        return -1; // Safety check; should not happen.
    }

    // Get the previous and next blocks for merging purposes.
    block_t *prev = __blkmngr_get_previous_block(header, block);
    block_t *next = __blkmngr_get_next_block(header, block);

    pr_debug("Freeing block %s\n", __block_to_string(block));

    // Mark the block as free.
    block->is_free = 1;

    // Attempt to merge with adjacent blocks if they are free.
    if (prev && prev->is_free && next && next->is_free) {
        pr_debug("Merging with previous and next blocks.\n");
        __blkmngr_merge_blocks(header, prev, block); // Merge with previous.
        __blkmngr_merge_blocks(header, prev, next);  // Merge with next.
    } else if (prev && prev->is_free) {
        pr_debug("Merging with previous block.\n");
        __blkmngr_merge_blocks(header, prev, block); // Merge with previous.
    } else if (next && next->is_free) {
        pr_debug("Merging with next block.\n");
        // Merge the blocks with the next one.
        __blkmngr_merge_blocks(header, block, next);
        // Add the current block to the free list.
        list_head_insert_before(&block->free, &header->free);
    } else {
        pr_debug("No merging required; adding block to free list.\n");
        // Add the block to the free list since no merging is needed.
        list_head_insert_before(&block->free, &header->free);
    }

    // Dump the current state of the heap for debugging purposes.
    __blkmngr_dump(header);

    return 0; // Return success.
}

void *sys_brk(void *addr)
{
    // Check if the address is NULL.
    if (!addr) {
        pr_err("Received a NULL addr.\n");
        return NULL; // Return error if the addr is NULL.
    }

    // Get the current process.
    task_struct *task = scheduler_get_current_process();
    if (!task) {
        pr_err("There is no current task!\n");
        return NULL; // Return error if there is no current task.
    }

    // Get the memory descriptor for the current task.
    mm_struct_t *mm = task->mm;
    if (!mm) {
        pr_err("The mm_struct of the current task is not initialized!\n");
        return NULL; // Return error if memory descriptor is not initialized.
    }

    // Get the heap associated with the current task.
    vm_area_struct_t *heap = find_vm_area(task->mm, task->mm->start_brk);

    // If the heap does not exist, allocate it.
    if (heap == NULL) {
        pr_debug("Allocating heap!\n");

        // Set the heap size to 4 MB.
        size_t heap_size = HEAP_SIZE;

        // Calculate the total segment size needed (header + initial block).
        size_t segment_size = heap_size + sizeof(heap_header_t) + sizeof(block_t);

        // Create the virtual memory area, we are goin to place the area between
        // 0x40000000 and 0x50000000, which surely is below the stack. The VM
        // code will check if it is a valid area anyway.
        heap = create_vm_area(
            task->mm,
            randuint(HEAP_VM_LB, HEAP_VM_UB),
            segment_size,
            MM_RW | MM_PRESENT | MM_USER | MM_UPDADDR,
            GFP_HIGHUSER);
        if (!heap) {
            pr_err("Failed to allocate heap memory area.\n");
            return NULL; // Return error if heap allocation fails.
        }

        pr_debug("Heap size  : %s.\n", to_human_size(heap_size));
        pr_debug("Heap start : 0x%p.\n", heap->vm_start);
        pr_debug("Heap end   : 0x%p.\n", heap->vm_end);

        // Initialize the memory for the heap.
        memset((char *)heap->vm_start, 0, segment_size);

        // Save the starting address of the heap.
        task->mm->start_brk = heap->vm_start;

        // Initialize the heap header.
        heap_header_t *header = (heap_header_t *)heap->vm_start;
        list_head_init(&header->list);
        list_head_init(&header->free);

        // Prepare the first block of memory.
        block_t *block = (block_t *)((char *)header + sizeof(heap_header_t));
        block->size    = K; // Start with a block of 1 KB.
        list_head_init(&block->list);
        list_head_init(&block->free);

        // Insert the initial block into the heap.
        list_head_insert_before(&block->list, &header->list);
        list_head_insert_before(&block->free, &header->free);

        // Set the actual starting address of the heap.
        task->mm->brk = (uint32_t)((char *)block + OVERHEAD + block->size);
        // Mark the initial block as free.
        block->is_free = 1;

        // Dump the state of the memory manager for debugging.
        __blkmngr_dump(header);
    }

    // Variable to hold the return pointer.
    void *_ret = NULL;

    // Check if the specified address is within the allocated heap range.
    if (((uintptr_t)addr > heap->vm_start) && ((uintptr_t)addr < heap->vm_end)) {
        // If it is, free the specified address.
        if (__do_free(heap, addr) < 0) {
            pr_err("Failed to free memory at address: 0x%p.\n", addr);
            return NULL; // Return error if freeing fails.
        }
    } else {
        // If not, allocate new memory of the specified size.
        _ret = __do_malloc(heap, (size_t)addr);
        if (!_ret) {
            pr_err("Memory allocation failed for size: %zu.\n", (size_t)addr);
            return NULL; // Return error if allocation fails.
        }
    }

    return _ret; // Return the pointer to the allocated memory or NULL on failure.
}
