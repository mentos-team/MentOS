/// @file wait.c
/// @brief wait functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[WAIT  ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "process/wait.h"
#include "mem/slab.h"
#include "assert.h"
#include "string.h"

/// @brief Adds the entry to the wait queue.
/// @param head the wait queue.
/// @param wq the entry.
static inline void __add_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *wq)
{
    list_head_insert_before(&wq->task_list, &head->task_list);
}

/// @brief Removes the entry from the wait queue.
/// @param head the wait queue.
/// @param wq the entry.
static inline void __remove_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *wq)
{
    list_head_remove(&wq->task_list);
}

wait_queue_entry_t *wait_queue_entry_alloc(void)
{
    // Allocate the memory.
    wait_queue_entry_t *wait_queue_entry = (wait_queue_entry_t *)kmalloc(sizeof(wait_queue_entry_t));
    pr_debug("ALLOCATE wait_queue_entry_t 0x%p\n", wait_queue_entry);
    // Check the allocated memory.
    assert(wait_queue_entry && "Failed to allocate memory for a wait_queue_entry_t.");
    // Clean the memory.
    memset(wait_queue_entry, 0, sizeof(wait_queue_entry_t));
    // Initialize the element.
    wait_queue_entry->flags = 0;
    wait_queue_entry->task  = NULL;
    wait_queue_entry->func  = NULL;
    list_head_init(&wait_queue_entry->task_list);
    // Return the element.
    return wait_queue_entry;
}

void wait_queue_entry_dealloc(wait_queue_entry_t *wait_queue_entry)
{
    assert(wait_queue_entry && "Received a NULL pointer.");
    pr_debug("FREE     wait_queue_entry_t 0x%p\n", wait_queue_entry);
    // Deallocate the memory.
    kfree(wait_queue_entry);
}

void init_waitqueue_entry(wait_queue_entry_t *wq, struct task_struct *task)
{
    wq->flags = 0;
    wq->task  = task;
    wq->func  = default_wake_function;
}

void add_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *wq)
{
    wq->flags &= ~WQ_FLAG_EXCLUSIVE;
    spinlock_lock(&head->lock);
    __add_wait_queue(head, wq);
    spinlock_unlock(&head->lock);
}

void remove_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *wq)
{
    spinlock_lock(&head->lock);
    __remove_wait_queue(head, wq);
    spinlock_unlock(&head->lock);
}
