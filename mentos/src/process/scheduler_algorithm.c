///                MentOS, The Mentoring Operating system project
/// @file scheduler_algorithm.c
/// @brief Round Robin algorithm.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "timer.h"
#include "prio.h"
#include "debug.h"
#include "assert.h"
#include "list_head.h"
#include "wait.h"
#include "scheduler.h"

static inline task_struct *scheduler_rr(runqueue_t *runqueue, bool_t skip_periodic)
{
    // If there is just one process, return it.
    if ((runqueue->curr->run_list.next == &runqueue->queue) &&
        (runqueue->curr->run_list.prev == &runqueue->queue)) {
        return runqueue->curr;
    }
    // By default, the next process is the current one.
    task_struct *next = NULL, *entry = NULL;
    // Search for the next process (BEWARE: We do not start from the head, so INSIDE skip the head).
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

        // Skip the process if it is a periodic one, we are issued to skip
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

static inline task_struct *scheduler_priority(runqueue_t *runqueue, bool_t skip_periodic)
{
    return scheduler_rr(runqueue, skip_periodic);
}

static inline task_struct *scheduler_cfs(runqueue_t *runqueue, bool_t skip_periodic)
{
    return scheduler_rr(runqueue, skip_periodic);
}

static inline task_struct *scheduler_aedf(runqueue_t *runqueue)
{
    return scheduler_rr(runqueue, false);
}

static inline task_struct *scheduler_edf(runqueue_t *runqueue)
{
    return scheduler_rr(runqueue, false);
}

static inline task_struct *scheduler_rm(runqueue_t *runqueue)
{
    return scheduler_rr(runqueue, false);
}

task_struct *scheduler_pick_next_task(runqueue_t *runqueue)
{
    // While periodic task is under analysis is executed with aperiodic
    // scheduler and can be preempted by a "true" periodic task.
    // We need to sum all the execution spots to calculate the WCET even
    // if is a more pessimistic evaluation.
    // Update the delta exec.
    runqueue->curr->se.exec_runtime = timer_get_ticks() - runqueue->curr->se.exec_start;
    update_process_profiling_timer(runqueue->curr);

    // set the sum_exec_runtime.
    runqueue->curr->se.sum_exec_runtime += runqueue->curr->se.exec_runtime;

    // If the task is not a periodic task we have to update the virtual runtime.
    if (!runqueue->curr->se.is_periodic) {
        // Get the weight of the current process.
        time_t weight = GET_WEIGHT(runqueue->curr->se.prio);
        if (weight != NICE_0_LOAD) {
            // get the multiplicative factor for its delta_exec.
            double factor = ((double)NICE_0_LOAD) / ((double)weight);
            // weight the delta_exec with the multiplicative factor.
            runqueue->curr->se.exec_runtime = (int)(((double)runqueue->curr->se.exec_runtime) * factor);
        }
        // Update vruntime of the current process.
        runqueue->curr->se.vruntime += runqueue->curr->se.exec_runtime;
    }

    // Pointer to the next task to schedule.
    task_struct *next = NULL;
#if defined(SCHEDULER_RR)
    next = scheduler_rr(runqueue, false);
#elif defined(SCHEDULER_PRIORITY)
    next = scheduler_priority(runqueue, false);
#elif defined(SCHEDULER_CFS)
    next = scheduler_cfs(runqueue, false);
#elif defined(SCHEDULER_EDF)
    next = scheduler_edf(runqueue);
#elif defined(SCHEDULER_RM)
    next = scheduler_rm(runqueue);
#elif defined(SCHEDULER_AEDF)
    next = scheduler_aedf(runqueue);
#else
#error "You should enable a scheduling algorithm!"
#endif
    assert(next && "No valid task selected by the scheduling algorithm.");

    // Update the last context switch time of the next process.
    next->se.exec_start = timer_get_ticks();

    return next;
}
