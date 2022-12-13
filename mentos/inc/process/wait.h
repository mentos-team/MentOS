/// @file wait.h
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "klib/list_head.h"
#include "klib/spinlock.h"

/// @brief Return immediately if no child is there to be waited for.
#define WNOHANG 0x00000001
/// @brief Return for children that are stopped, and whose status has not
///        been reported.
#define WUNTRACED 0x00000002
/// @brief returns true if the child process exited because of a signal that
///        was not caught.
#define WIFSIGNALED(status) (!WIFSTOPPED(status) && !WIFEXITED(status))
/// @brief returns true if the child process that caused the return is
///        currently stopped; this is only possible if the call was done using
///        WUNTRACED().
#define WIFSTOPPED(status) (((status)&0xff) == 0x7f)
/// @brief evaluates to the least significant eight bits of the return code
///        of the child that terminated, which may have been set as the argument
///        to a call to exit() or as the argument for a return statement in the
///        main  program. This macro can only be evaluated if WIFEXITED()
///        returned nonzero.
#define WEXITSTATUS(status) (((status)&0xff00) >> 8)
/// @brief returns the number of the signal that caused the child process to
///        terminate. This macro can only be evaluated if WIFSIGNALED() returned
///        nonzero.
#define WTERMSIG(status) ((status)&0x7f)
/// @brief Is nonzero if the child exited normally.
#define WIFEXITED(status) (WTERMSIG(status) == 0)
/// @brief returns the number of the signal that caused the child to stop.
///        This macro can only be evaluated if WIFSTOPPED() returned nonzero.
#define WSTOPSIG(status) (WEXITSTATUS(status))

//==== Task States ============================================================
#define TASK_RUNNING         0x00     ///< The process is either: 1) running on CPU or 2) waiting in a run queue.
#define TASK_INTERRUPTIBLE   (1 << 0) ///< The process is sleeping, waiting for some event to occur.
#define TASK_UNINTERRUPTIBLE (1 << 1) ///< Similar to TASK_INTERRUPTIBLE, but it doesn't process signals.
#define TASK_STOPPED         (1 << 2) ///< Stopped, it's not running, and not able to run.
#define TASK_TRACED          (1 << 3) ///< Is being monitored by other processes such as debuggers.
#define EXIT_ZOMBIE          (1 << 4) ///< The process has terminated.
#define EXIT_DEAD            (1 << 5) ///< The final state.
//==============================================================================

/// @defgroup WaitQueueFlags Wait Queue Flags
/// @{

/// @brief When an entry has this flag is added to the end of the wait queue.
///        Entries without that flag are, instead, added to the beginning.
#define WQ_FLAG_EXCLUSIVE 0x01
//#define WQ_FLAG_WOKEN     0x02
//#define WQ_FLAG_BOOKMARK  0x04
//#define WQ_FLAG_CUSTOM    0x08
//#define WQ_FLAG_DONE      0x10

/// @}

/// @brief Head of the waiting queue.
typedef struct wait_queue_head_t {
    /// Locking element for the waiting queque.
    spinlock_t lock;
    /// Head of the waiting queue, it contains wait_queue_entry_t elements.
    struct list_head task_list;
} wait_queue_head_t;

/// @brief Entry of the waiting queue.
typedef struct wait_queue_entry_t {
    /// Flags of the type WaitQueueFlags.
    unsigned int flags;
    /// Task associated with the wait queue entry.
    struct task_struct *task;
    /// Function associated with the wait queue entry.
    int (*func)(struct wait_queue_entry_t *wait, unsigned mode, int sync);
    /// Handler for placing the entry inside a waiting queue double linked-list.
    struct list_head task_list;
} wait_queue_entry_t;

/// @brief Initialize the waiting queue entry.
/// @param wq   The entry we initialize.
/// @param task The task associated with the entry.
void init_waitqueue_entry(wait_queue_entry_t *wq, struct task_struct *task);

/// @brief Adds the element to the waiting queue.
/// @param head The head of the waiting queue.
/// @param wq   The entry we insert inside the waiting queue.
void add_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *wq);

/// @brief Removes the element from the waiting queue.
/// @param head The head of the waiting queue.
/// @param wq   The entry we remove from the waiting queue.
void remove_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *wq);

/// @brief The default wake function, a wrapper for try_to_wake_up.
/// @param wait The pointer to the wait queue.
/// @param mode The type of wait (TASK_INTERRUPTIBLE or TASK_UNINTERRUPTIBLE).
/// @param sync Specifies if the wakeup should be synchronous.
/// @return 1 on success, 0 on failure.
int default_wake_function(wait_queue_entry_t *wait, unsigned mode, int sync);

/// @brief Sets the state of the current process to TASK_UNINTERRUPTIBLE 
///        and inserts it into the specified wait queue.
/// 
/// @param wq Waitqueue where to sleep.
/// @return Pointer to the entry inside the wq representing the 
///         sleeping process.
wait_queue_entry_t *sleep_on(wait_queue_head_t *wq);
