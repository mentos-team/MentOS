/// @file wait.c
/// @brief wait functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[WAIT  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "process/wait.h"

#include "assert.h"
#include "mem/alloc/slab.h"
#include "process/scheduler.h"
#include "string.h"

/// @brief Adds the entry to the wait queue.
/// @param head the wait queue.
/// @param entry the entry.
static inline void __add_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *entry)
{
    // Validate the input.
    if (!head) {
        pr_err("Variable head is NULL.\n");
        return;
    }
    if (!entry) {
        pr_err("Variable entry is NULL.\n");
        return;
    }
    list_head_insert_before(&entry->task_list, &head->task_list);
}

/// @brief Removes the entry from the wait queue.
/// @param head the wait queue.
/// @param entry the entry.
static inline void __remove_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *entry)
{
    // Validate the input.
    if (!head) {
        pr_err("Variable head is NULL.\n");
        return;
    }
    if (!entry) {
        pr_err("Variable entry is NULL.\n");
        return;
    }
    list_head_remove(&entry->task_list);
}

int default_wake_function(wait_queue_entry_t *entry, unsigned mode, int sync)
{
    // Validate the input.
    if (!entry) {
        pr_err("Variable entry is NULL.\n");
        return 0;
    }
    if (!entry->task) {
        pr_err("Variable entry->task is NULL.\n");
        return 0;
    }
    // Only wake up tasks in TASK_INTERRUPTIBLE or TASK_UNINTERRUPTIBLE states.
    if ((entry->task->state == TASK_INTERRUPTIBLE) || (entry->task->state == TASK_UNINTERRUPTIBLE)) {
        // Set the task state to the specified mode.
        entry->task->state = mode;

        // Optionally handle sync-specific operations here if needed.
        // For now, sync is unused.

        return 1;
    }

    // Task is not in a wakeable state.
    return 0;
}

void wait_queue_head_init(wait_queue_head_t *head)
{
    // Validate the input.
    if (!head) {
        pr_err("Variable head is NULL.\n");
        return;
    }
    // Initialize the spinlock for the wait queue.
    spinlock_init(&head->lock);
    // Initialize the task list as an empty list.
    list_head_init(&head->task_list);
    pr_debug("Initialized wait queue head at %p.\n", head);
}

wait_queue_entry_t *wait_queue_entry_alloc(void)
{
    // Allocate the memory.
    wait_queue_entry_t *entry = (wait_queue_entry_t *)kmalloc(sizeof(wait_queue_entry_t));
    pr_debug("ALLOCATE wait_queue_entry_t 0x%p\n", entry);
    // Check the allocated memory.
    assert(entry && "Failed to allocate memory for a wait_queue_entry_t.");
    // Clean the memory.
    memset(entry, 0, sizeof(wait_queue_entry_t));
    // Initialize the element.
    entry->flags   = 0;
    entry->task    = NULL;
    entry->func    = NULL;
    entry->private = NULL;
    list_head_init(&entry->task_list);
    // Return the element.
    return entry;
}

void wait_queue_entry_dealloc(wait_queue_entry_t *entry)
{
    assert(entry && "Received a NULL pointer.");
    pr_debug("FREE     wait_queue_entry_t 0x%p\n", entry);
    // Deallocate the memory.
    kfree(entry);
}

void wait_queue_entry_init(wait_queue_entry_t *entry, struct task_struct *task)
{
    // Validate the input.
    if (!entry) {
        pr_err("Variable entry is NULL.\n");
        return;
    }
    if (!task) {
        pr_err("Variable head is NULL.\n");
        return;
    }
    entry->flags   = 0;
    entry->task    = task;
    entry->func    = default_wake_function;
    entry->private = NULL;
    list_head_init(&entry->task_list);
}

void add_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *entry)
{
    // Validate the input.
    if (!head) {
        pr_err("Variable head is NULL.\n");
        return;
    }
    if (!entry) {
        pr_err("Variable entry is NULL.\n");
        return;
    }
    entry->flags &= ~WQ_FLAG_EXCLUSIVE;
    spinlock_lock(&head->lock);
    __add_wait_queue(head, entry);
    spinlock_unlock(&head->lock);
}

void remove_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *entry)
{
    // Validate the input.
    if (!head) {
        pr_err("Variable head is NULL.\n");
        return;
    }
    if (!entry) {
        pr_err("Variable entry is NULL.\n");
        return;
    }
    spinlock_lock(&head->lock);
    __remove_wait_queue(head, entry);
    spinlock_unlock(&head->lock);
}

wait_queue_entry_t *sleep_on(wait_queue_head_t *head)
{
    // Validate input parameters.
    if (!head) {
        pr_err("Wait queue head is NULL.\n");
        return NULL;
    }

    // Retrieve the current process/task.
    task_struct *sleeping_task = scheduler_get_current_process();
    if (!sleeping_task) {
        pr_err("Failed to retrieve the current process.\n");
        return NULL;
    }

    // Set the task state to uninterruptible to indicate it is sleeping.
    sleeping_task->state = TASK_UNINTERRUPTIBLE;

    // Allocate memory for a new wait queue entry.
    wait_queue_entry_t *entry = wait_queue_entry_alloc();
    if (!entry) {
        pr_err("Failed to allocate memory for wait queue entry.\n");
        return NULL;
    }

    // Initialize the wait queue entry with the current task.
    wait_queue_entry_init(entry, sleeping_task);

    // Add the wait queue entry to the specified wait queue.
    add_wait_queue(head, entry);

    pr_debug("Added process %d to the wait queue.\n", sleeping_task->pid);

    return entry;
}
