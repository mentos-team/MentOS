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
#include "klib/irqflags.h"
#include "mem/alloc/slab.h"
#include "process/scheduler.h"
#include "string.h"
#include <stdint.h>

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
    // pr_debug("ALLOCATE wait_queue_entry_t %p\n", entry);
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
    // pr_debug("FREE     wait_queue_entry_t %p\n", entry);
    // Deallocate the memory.
    kfree(entry);
}

void wake_up_all(wait_queue_head_t *head)
{
    if (!head) {
        pr_err("wake_up_all: head is NULL\n");
        return;
    }

    // iterate through the queue and invoke each wake function
    list_for_each_safe_decl(it, store, &head->task_list)
    {
        wait_queue_entry_t *entry = list_entry(it, wait_queue_entry_t, task_list);
        if (entry->func(entry, TASK_RUNNING, 0) || entry->task->state == TASK_RUNNING) {
            // Debug on the output.
            pr_debug("Process %d (%s) WOKEN UP from %s\n", entry->task->pid, entry->task->name, head->name);
            // Remove the entry from the wait queue and re-enqueue the task if it's not already running.
            remove_wait_queue(head, entry);
            // Clear the wait queue tracking field when removing from queue
            entry->task->waiting_on = NULL;
            /* only enqueue if not already on runqueue */
            if (list_head_empty(&entry->task->run_list)) {
                scheduler_enqueue_task(entry->task);
            }
            wait_queue_entry_dealloc(entry);
        }
    }
}

int wake_up_process_on_queue(wait_queue_head_t *head, struct task_struct *task)
{
    if (!head) {
        pr_err("wake_up_process_on_queue: head is NULL\n");
        return 0;
    }
    if (!task) {
        pr_err("wake_up_process_on_queue: task is NULL\n");
        return 0;
    }

    // Search for the specific task in the wait queue
    list_for_each_safe_decl(it, store, &head->task_list)
    {
        wait_queue_entry_t *entry = list_entry(it, wait_queue_entry_t, task_list);
        // Check if this entry corresponds to our target task
        if (entry->task == task) {
            // Try to wake up the task using its wake function
            if (entry->func(entry, TASK_RUNNING, 0) || entry->task->state == TASK_RUNNING) {
                pr_debug("Process %d (%s) WOKEN UP from %s\n", entry->task->pid, entry->task->name, head->name);
                // Remove the entry from the wait queue
                remove_wait_queue(head, entry);
                // Clear the wait queue tracking field
                entry->task->waiting_on = NULL;
                // Re-enqueue the task if it's not already on the runqueue
                if (list_head_empty(&entry->task->run_list)) {
                    scheduler_enqueue_task(entry->task);
                }
                wait_queue_entry_dealloc(entry);
                return 1; // Successfully woken up
            }
        }
    }

    // Task was not found in this wait queue
    return 0;
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

    pr_debug("Removed process %d (%s) from wait queue %s (entry %p)\n", entry->task->pid, entry->task->name, head->name, entry);
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

    // We want to avoid a race where an interrupt arrives between setting the task state to TASK_UNINTERRUPTIBLE and the
    // caller adding the entry to the queue.  If the wakeup occurs in that window the notification is lost and the task
    // could sleep forever.  The simple way to prevent this is to disable interrupts while changing the state and
    // inserting the entry; IRQs are restored before returning so normal operation resumes.
    uint8_t irqs = irq_disable();

    wait_queue_entry_t *entry = wait_queue_entry_alloc();
    if (!entry) {
        pr_err("Failed to allocate memory for wait queue entry.\n");
        irq_enable(irqs);
        return NULL;
    }

    // Set the task state to uninterruptible to indicate it is sleeping.
    sleeping_task->state = TASK_UNINTERRUPTIBLE;

    // Track which wait queue this task is sleeping on.
    sleeping_task->waiting_on = head;

    // Remove task from runqueue so scheduler will ignore it while blocked.
    scheduler_dequeue_task(sleeping_task);

    // Initialize the wait queue entry with the current task.
    wait_queue_entry_init(entry, sleeping_task);

    // Add the wait queue entry to the specified wait queue.
    add_wait_queue(head, entry);

    /* restore interrupts before returning */
    irq_enable(irqs);

    pr_debug("Process %d (%s) SLEEPS ON %s (entry %p)\n", sleeping_task->pid, sleeping_task->name, head->name, entry);

    return entry;
}
