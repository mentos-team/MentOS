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
#include "process/scheduler.h"
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
    /// removing from queue and calling wake_up_process(). The waiter owns the
    /// entry storage and releases it when its continuation calls finish_wait().

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
        pr_debug("Task %d (%s) wake condition met (state: %ld)\n", 
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
    pr_debug("Initialized wait queue head at %p.\n", (void *)head);
}

int wake_up_wait_queue_entry(wait_queue_head_t *head, wait_queue_entry_t *entry, unsigned mode, int sync)
{
    /// Core wait queue wake primitive. Evaluates wake condition via entry's
    /// wake function, and if satisfied, performs the full wake sequence:
    ///   1. Remove entry from wait queue (wait layer responsibility)
    ///   2. Call wake_up_process() to transition task to TASK_RUNNING
    ///   3. Leave entry storage owned by the blocked continuation
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

    // Perform wake sequence: remove and make the task runnable. The blocked
    // continuation owns the entry and will finish it after schedule() returns.
    remove_wait_queue(head, entry);
    wake_up_process(entry->task);
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

void prepare_to_wait(wait_queue_head_t *head, wait_queue_entry_t *entry, long state)
{
    if (head == NULL || entry == NULL || entry->task == NULL) {
        return;
    }
    uint8_t irqs = irq_disable();
    spinlock_lock(&head->lock);
    entry->task->state = state;
    entry->task->waiting_on = head;
    if (list_head_empty(&entry->task_list)) {
        __add_wait_queue(head, entry);
    }
    spinlock_unlock(&head->lock);
    irq_enable(irqs);
}

void finish_wait(wait_queue_head_t *head, wait_queue_entry_t *entry)
{
    if (head == NULL || entry == NULL || entry->task == NULL) {
        return;
    }
    uint8_t irqs = irq_disable();
    spinlock_lock(&head->lock);
    if (!list_head_empty(&entry->task_list)) {
        __remove_wait_queue(head, entry);
    }
    if (entry->task->waiting_on == head) {
        entry->task->waiting_on = NULL;
    }
    if (entry->task->state == TASK_INTERRUPTIBLE || entry->task->state == TASK_UNINTERRUPTIBLE) {
        entry->task->state = TASK_RUNNING;
    }
    spinlock_unlock(&head->lock);
    irq_enable(irqs);
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

    pr_debug("Removed process %d (%s) from wait queue %s (state: %ld)\n", entry->task->pid, entry->task->name, head->name, entry->task->state);
}
