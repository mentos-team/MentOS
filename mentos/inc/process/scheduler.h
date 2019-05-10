///                MentOS, The Mentoring Operating system project
/// @file scheduler.h
/// @brief Scheduler structures and functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "clock.h"
#include "kernel.h"
#include "stdint.h"
#include "process.h"

typedef struct {
	/// Number of queued processes.
	size_t num_active;

	/// Queue of processes.
	list_head queue;

	/// The current running process.
	task_struct *curr;
} runqueue_t;

/// @brief  Returns a non-decreasing unique process id.
/// @return Process identifier (PID).
uint32_t get_new_pid();

/// @brief Initialize the scheduler.
void kernel_initialize_scheduler();

/// @brief         Activate the given process.
/// @param process Process that has to be activated.
void enqueue_task(task_struct *process);

/// @brief         Removes the given process from the queue.
/// @param process Process that has to be activated.
void dequeue_task(task_struct *process);

/// @brief Returns the number of active processes.
size_t kernel_get_active_processes();

/// @brief Returns the pointer to the current active process.
task_struct *kernel_get_current_process();

/// @brief Returns a pointer to the process with the given pid.
task_struct *kernel_get_running_process(pid_t pid);

task_struct *pick_next_task(runqueue_t *runqueue, time_t delta_exec);

/// @brief   The RR implementation of the scheduler.
/// @param f The context of the process.
void kernel_schedule(pt_regs *f);

/// @birief Values from pt_regs to task_struct process.
void update_context(pt_regs *f, task_struct *process);

/// @brief Values from task_struct process to pt_regs.
void do_switch(task_struct *process, pt_regs *f);

/// @brief         Switch CPU to user mode and start running that given process.
/// @param process The process that has to be executed
void enter_user_jmp(uintptr_t location, uintptr_t stack);

/// @brief Sets the priority value of the given task.
int set_user_nice(task_struct *p, long nice);


