/// @file scheduler_algorithm.c
/// @brief Round Robin algorithm.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
// Change the header.
#define __DEBUG_HEADER__ "[SCHALG]"
// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "hardware/timer.h"
#include "process/prio.h"
#include "io/debug.h"
#include "assert.h"
#include "klib/list_head.h"
#include "process/wait.h"
#include "process/scheduler.h"

/// @brief Updates task execution statistics.
/// @param task the task to update.
static void __update_task_statistics(task_struct *task);

/// @brief Employs time-sharing, giving each job a timeslice, and is also
/// preemptive since the scheduler forces the task out of the CPU once
/// the timeslice expires.
/// @param runqueue list of all processes.
/// @param skip_periodic tells the algorithm if there are periodic processes
///                      in the list, and in that case it needs to skip them.
/// @return the next task on success, NULL on failure.
static inline task_struct *__scheduler_rr(runqueue_t *runqueue, bool_t skip_periodic)
{
    // If there is just one task, return it.
    if ((runqueue->curr->run_list.next == &runqueue->queue) &&
        (runqueue->curr->run_list.prev == &runqueue->queue)) {
        return runqueue->curr;
    }
    // By default, the next task is the current one.
    task_struct *next = NULL, *entry = NULL;
    // Search for the next task (BEWARE: We do not start from the head, so INSIDE skip the head).
    list_for_each_decl(it, &runqueue->curr->run_list)
    {
        // Check if we reached the head of list_head, and skip it.
        if (it == &runqueue->queue)
            continue;
        // Get the current entry.
        entry = list_entry(it, task_struct, run_list);

        // We consider only runnable processes
        if (entry->state != TASK_RUNNING)
            continue;

        // Skip the task if it is a periodic one, we are issued to skip
        // periodic tasks, and the entry is not a periodic task under
        // analysis.
        if (entry->se.is_periodic && skip_periodic && !entry->se.is_under_analysis)
            continue;

        // We have our next entry.
        next = entry;
        break;
    }
    return next;
}

/// @brief Is a non-preemptive algorithm, where each task is assigned a
/// priority. Processes with highest priority are executed first, while
/// processes with same priority are executed on first-come/first-served
/// basis. Priority can be decided based on memory requirements, time
/// requirements or any other resource requirement.
/// @param runqueue list of all processes.
/// @param skip_periodic tells the algorithm if there are periodic processes
///                      in the list, and in that case it needs to skip them.
/// @return the next task on success, NULL on failure.
static inline task_struct *__scheduler_priority(runqueue_t *runqueue, bool_t skip_periodic)
{
    return __scheduler_rr(runqueue, skip_periodic);
}

/// @brief It aims at giving a fair share of CPU time to processes, and
/// achieves that by associating a virtual runtime to each of them. It always
/// tries to run the task with the smallest vruntime (i.e., the task which
/// executed least so far). It always tries to split up CPU time between
/// runnable tasks as close to "ideal multitasking hardware" as possible.
/// @param runqueue list of all processes.
/// @param skip_periodic tells the algorithm if there are periodic processes
///                      in the list, and in that case it needs to skip them.
/// @return the next task on success, NULL on failure.
static inline task_struct *__scheduler_cfs(runqueue_t *runqueue, bool_t skip_periodic)
{
    return __scheduler_rr(runqueue, skip_periodic);
}

/// @brief Executes the task with the earliest absolute deadline among all
/// the ready tasks.
/// @param runqueue list of all processes.
/// @return the next task on success, NULL on failure.
static inline task_struct *__scheduler_aedf(runqueue_t *runqueue)
{
    return __scheduler_rr(runqueue, false);
}

/// @brief Executes the task with the earliest absolute DEADLINE among all
/// the ready tasks. When a task was executed, and its period is starting
/// again, it must be set as 'executable again', and its deadline and next_period
/// must be updated.
/// @param runqueue list of all processes.
/// @return the next task on success, NULL on failure.
static inline task_struct *__scheduler_edf(runqueue_t *runqueue)
{
    return __scheduler_rr(runqueue, false);
}

/// @brief Executes the task with the earliest next PERIOD among all the
/// ready tasks.
/// @details When a task was executed, and its period is starting again, it
/// must be set as 'executable again', and its deadline and next_period must
/// be updated.
/// @param runqueue list of all processes.
/// @return the next task on success, NULL on failure.
static inline task_struct *__scheduler_rm(runqueue_t *runqueue)
{
    return __scheduler_rr(runqueue, false);
}

task_struct *scheduler_pick_next_task(runqueue_t *runqueue)
{
    // Update task statistics.
    __update_task_statistics(runqueue->curr);

    // Pointer to the next task to schedule.
    task_struct *next = NULL;
#if defined(SCHEDULER_RR)
    next = __scheduler_rr(runqueue, false);
#elif defined(SCHEDULER_PRIORITY)
    next = __scheduler_priority(runqueue, false);
#elif defined(SCHEDULER_CFS)
    next = __scheduler_cfs(runqueue, false);
#elif defined(SCHEDULER_EDF)
    next = __scheduler_edf(runqueue);
#elif defined(SCHEDULER_RM)
    next = __scheduler_rm(runqueue);
#elif defined(SCHEDULER_AEDF)
    next = __scheduler_aedf(runqueue);
#else
#error "You should enable a scheduling algorithm!"
#endif

    assert(next && "No valid task selected by the scheduling algorithm.");

    // Update the last context switch time of the next task.
    next->se.exec_start = timer_get_ticks();

    return next;
}

static void __update_task_statistics(task_struct *task)
{
    assert(task && "Current task is not valid.");

    // While periodic task is under analysis is executed with aperiodic
    // scheduler and can be preempted by a "true" periodic task.
    // We need to sum all the execution spots to calculate the WCET even
    // if is a more pessimistic evaluation.
    // Update the delta exec.
    task->se.exec_runtime = timer_get_ticks() - task->se.exec_start;
    update_process_profiling_timer(task);

    // Set the sum_exec_runtime.
    task->se.sum_exec_runtime += task->se.exec_runtime;

    // If the task is not a periodic task we have to update the virtual runtime.
    if (!task->se.is_periodic) {
        // Get the weight of the current task.
        time_t weight = GET_WEIGHT(task->se.prio);
        // If the weight is different from the default load, compute it.
        if (weight != NICE_0_LOAD) {
            // Get the multiplicative factor for its delta_exec.
            double factor = ((double)NICE_0_LOAD) / ((double)weight);
            // Weight the delta_exec with the multiplicative factor.
            task->se.exec_runtime = (int)(((double)task->se.exec_runtime) * factor);
        }
        // Update vruntime of the current task.
        task->se.vruntime += task->se.exec_runtime;
    }
}