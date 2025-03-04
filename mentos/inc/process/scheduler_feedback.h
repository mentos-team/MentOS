/// @file scheduler_feedback.h
/// @brief Scheduler feedback system for managing tasks and updating scheduling statistics.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "process/process.h"
#include "sys/types.h"

/// @brief Initialize the scheduler feedback system.
/// @details This function sets up the necessary data structures and mechanisms
/// for the scheduler feedback system. It must be called before any other
/// scheduler feedback operations are performed.
/// @return 1 on success, 0 on failure.
int scheduler_feedback_init(void);

/// @brief Add the given task to the feedback system.
/// @param task A pointer to the task_struct representing the task to be added.
/// @details This function adds a task to the scheduler feedback system for
/// monitoring and updating its scheduling statistics.
void scheduler_feedback_task_add(task_struct *task);

/// @brief Removes the given task from the feedback system.
/// @param pid The process ID of the task to remove.
/// @details This function removes the task identified by the given pid from the
/// scheduler feedback system. It should be called when a task is terminated or
/// no longer needs to be monitored.
void scheduler_feedback_task_remove(pid_t pid);

/// @brief Updates the scheduling statistics for the given task.
/// @param task A pointer to the task_struct representing the task to update.
/// @details This function updates the scheduling statistics for the given task
/// based on its recent behavior (e.g., execution time, priority changes).
void scheduler_feedback_task_update(task_struct *task);

/// @brief Updates the global scheduler feedback statistics.
/// @details This function is periodically called to update the overall
/// statistics of the scheduler feedback system, adjusting task priorities and
/// managing scheduling decisions for all tasks.
void scheduler_feedback_update(void);
