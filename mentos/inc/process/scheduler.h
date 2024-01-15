/// @file scheduler.h
/// @brief Scheduler structures and functions.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/list_head.h"
#include "process/process.h"
#include "stddef.h"

/// @brief Structure that contains information about live processes.
typedef struct runqueue_t {
    /// Number of queued processes.
    size_t num_active;
    /// Number of queued periodic processes.
    size_t num_periodic;
    /// Queue of processes.
    list_head queue;
    /// The current running process.
    task_struct *curr;
} runqueue_t;

/// @brief Structure that describes scheduling parameters.
typedef struct sched_param_t {
    /// Static execution priority.
    int sched_priority;
    /// Expected period of the task
    time_t period;
    /// Absolute deadline
    time_t deadline;
    /// Absolute time of arrival of the task
    time_t arrivaltime;
    /// Is task periodic?
    bool_t is_periodic;
} sched_param_t;

/// @brief Initialize the scheduler.
void scheduler_initialize(void);

/// @brief  Returns a non-decreasing unique process id.
/// @return Process identifier (PID).
uint32_t scheduler_getpid(void);

/// @brief Returns the pointer to the current active process.
/// @return Pointer to the current process.
task_struct *scheduler_get_current_process(void);

/// @brief Returns the maximum vruntime of all the processes in running state.
/// @return A maximum vruntime value.
time_t scheduler_get_maximum_vruntime(void);

/// @brief Returns the number of active processes.
/// @return Number of processes.
size_t scheduler_get_active_processes(void);

/// @brief Returns a pointer to the process with the given pid.
/// @param pid The pid of the process we are looking for.
/// @return Pointer to the process, or NULL if we cannot find it.
task_struct *scheduler_get_running_process(pid_t pid);

/// @brief Activate the given process.
/// @param process Process that has to be activated.
void scheduler_enqueue_task(task_struct *process);

/// @brief Removes the given process from the queue.
/// @param process Process that has to be activated.
void scheduler_dequeue_task(task_struct *process);

/// @brief The RR implementation of the scheduler.
/// @param f The context of the process.
void scheduler_run(pt_regs *f);

/// @brief Values from pt_regs to task_struct process.
/// @param f       The set of registers we are saving.
/// @param process The process for which we are saving the CPU registers status.
void scheduler_store_context(pt_regs *f, task_struct *process);

/// @brief Values from task_struct process to pt_regs.
/// @param process The process for which we are restoring the registers in CPU .
/// @param f       The set of registers we are restoring.
void scheduler_restore_context(task_struct *process, pt_regs *f);

/// @brief Switch CPU to user mode and start running that given process.
/// @param location The instruction pointer of the process we are starting.
/// @param stack    Address of the stack of that process.
void scheduler_enter_user_jmp(uintptr_t location, uintptr_t stack);

/// @brief Picks the next task (in scheduler_algorithm.c).
/// @param runqueue   Pointer to the runqueue.
/// @return The next task to execute.
task_struct *scheduler_pick_next_task(runqueue_t *runqueue);

/// @brief Set new scheduling settings for the given process.
/// @param pid   ID of the process we are manipulating.
/// @param param New parameters.
/// @return 1 on success, -1 on error.
int sys_sched_setparam(pid_t pid, const sched_param_t *param);

/// @brief Gets the scheduling settings for the given process.
/// @param pid   ID of the process we are manipulating.
/// @param param Where we store the parameters.
/// @return 1 on success, -1 on error.
int sys_sched_getparam(pid_t pid, sched_param_t *param);

/// @brief Puts the process on wait until its next period starts.
/// @return 0 on success, a negative value on failure.
int sys_waitperiod(void);

/// @brief Returns 1 if the given group is orphaned, the session leader of the group
/// is no longer alive.
/// @param gid ID of the group
/// @return 1 if the group is orphan, 0 otherwise.
int is_orphaned_pgrp(pid_t gid);

/// @brief Exit the current process with status
/// @param status The exit status of the current process
void do_exit(int status);
