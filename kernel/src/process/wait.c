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
    /// Default wake predicate for wait queue entries.
    /// Returns 1 if the task is in a sleep state and should be woken,
    /// 0 if it should remain waiting.
    ///
    /// Wake functions are policy: they decide whether a task's wait
    /// condition is satisfied. The wait.c layer handles mechanics:
    /// removing from queue, calling wake_up_process(), freeing entry.

    // Validate the input.
    if (!entry) {
        pr_err("Variable entry is NULL.\n");
        return 0;
    }
    if (!entry->task) {
        pr_err("Variable entry->task is NULL.\n");
        return 0;
    }

    // Wake if task is in a sleep state (interruptible or uninterruptible).
    if ((entry->task->state == TASK_INTERRUPTIBLE) || 
        (entry->task->state == TASK_UNINTERRUPTIBLE)) {
        pr_debug("Task %d (%s) wake condition met (state: %d)\n", 
                 entry->task->pid, entry->task->name, entry->task->state);
        return 1;
    }

    // Task is not in a wakeable state (already running, stopped, or zombie).
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

int wake_up_wait_queue_entry(wait_queue_head_t *head, wait_queue_entry_t *entry, unsigned mode, int sync)
{
    /// Core wait queue wake primitive. Evaluates wake condition via entry's
    /// wake function, and if satisfied, performs the full wake sequence:
    ///   1. Remove entry from wait queue (wait layer responsibility)
    ///   2. Call wake_up_process() to transition task to TASK_RUNNING
    ///   3. Free wait queue entry (wait layer responsibility)
    ///
    /// This is the ONLY function that should perform this sequence.
    /// Subsystems (pipes, timers) delegate full wake mechanics here.

    if (!head) {
        pr_err("wake_up_wait_queue_entry: head is NULL\n");
        return 0;
    }
    if (!entry) {
        pr_err("wake_up_wait_queue_entry: entry is NULL\n");
        return 0;
    }
    if (!entry->task) {
        pr_err("wake_up_wait_queue_entry: entry->task is NULL\n");
        return 0;
    }

    // Evaluate wake condition via function pointer (policy decision).
    int should_wake = 0;
    if (entry->func) {
        should_wake = entry->func(entry, mode, sync);
    } else {
        should_wake = default_wake_function(entry, mode, sync);
    }

    // Also wake if task is already TASK_RUNNING (race or redundant wake).
    if (!should_wake && (entry->task->state != TASK_RUNNING)) {
        return 0;
    }

    pr_debug("Process %d (%s) WOKEN UP from %s\n", 
             entry->task->pid, entry->task->name, head->name);

    // Perform wake sequence: remove, wake, free (wait layer mechanics).
    remove_wait_queue(head, entry);
    wake_up_process(entry->task);
    wait_queue_entry_dealloc(entry);
    return 1;
}

void wake_up_all(wait_queue_head_t *head)
{
    if (!head) {
        pr_err("wake_up_all: head is NULL\n");
        return;
    }

    // Iterate through the queue and wake each task.
    // Each subsystem (pipes, signals, timers) owns its wait queue entries.
    // This function: removes entries, calls wake_up_process() to handle scheduling.
    list_for_each_safe_decl(it, store, &head->task_list)
    {
        wait_queue_entry_t *entry = list_entry(it, wait_queue_entry_t, task_list);
        wake_up_wait_queue_entry(head, entry, TASK_RUNNING, 0);
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

    // Search for the specific task in the wait queue.
    list_for_each_safe_decl(it, store, &head->task_list)
    {
        wait_queue_entry_t *entry = list_entry(it, wait_queue_entry_t, task_list);

        // Check if this entry corresponds to our target task.
        if (entry->task == task) {
            return wake_up_wait_queue_entry(head, entry, TASK_RUNNING, 0);
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

    pr_debug("Removed process %d (%s) from wait queue %s (state: %d)\n", entry->task->pid, entry->task->name, head->name, entry->task->state);
}

static wait_queue_entry_t *__sleep_on_state(wait_queue_head_t *head, int state)
{
    // Validate input parameters.
    if (!head) {
        pr_err("Wait queue head is NULL.\n");
        return NULL;
    }

    // We want to avoid a race where an interrupt arrives between setting the task state to TASK_UNINTERRUPTIBLE and the
    // caller adding the entry to the queue.  If the wakeup occurs in that window the notification is lost and the task
    // could sleep forever.  The simple way to prevent this is to disable interrupts while changing the state and
    // inserting the entry; IRQs are restored before returning so normal operation resumes.
    uint8_t irqs = irq_disable();

    // Retrieve the current process/task.
    task_struct *sleeping_task = scheduler_get_current_process();
    if (!sleeping_task) {
        pr_err("Failed to retrieve the current process.\n");
        return NULL;
    }

    wait_queue_entry_t *entry = wait_queue_entry_alloc();
    if (!entry) {
        pr_err("Failed to allocate memory for wait queue entry.\n");
        irq_enable(irqs);
        return NULL;
    }

    // Set the task state to indicate it is sleeping.
    // Task remains on runqueue - scheduler will skip it when picking next task.
    sleeping_task->state = state;

    // Track which wait queue this task is sleeping on.
    sleeping_task->waiting_on = head;

    // Initialize the wait queue entry with the current task.
    wait_queue_entry_init(entry, sleeping_task);

    // Add the wait queue entry to the specified wait queue.
    add_wait_queue(head, entry);

    pr_debug("Process %d (%s) SLEEPS ON %s (state: %d, stays on runqueue)\n", sleeping_task->pid, sleeping_task->name, head->name, sleeping_task->state);

    // Restore interrupts before returning.
    irq_enable(irqs);

    return entry;
}

wait_queue_entry_t *sleep_on(wait_queue_head_t *head)
{
    return __sleep_on_state(head, TASK_UNINTERRUPTIBLE);
}

wait_queue_entry_t *sleep_on_interruptible(wait_queue_head_t *head)
{
    return __sleep_on_state(head, TASK_INTERRUPTIBLE);
}
