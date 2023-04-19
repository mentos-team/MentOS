/// @file scheduler_feedback.h
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief 

#pragma once

#include "sys/types.h"
#include "process/process.h"

/// @brief Updates the scheduling statistics for the given task.
/// @param task the task for which we update the statistics.
void scheduler_feedback_update();

/// @brief Add the given task to the feedback system.
/// @param task the task we need to add. 
void scheduler_feedback_task_add(task_struct * task);

/// @brief Removes the given task from the feedback system.
/// @param pid the pid of the task we need to remove.
void scheduler_feedback_task_remove(pid_t pid);

/// @brief Updates the scheduling statistics for the given task.
/// @param task the task for which we update the statistics.
void scheduler_feedback_task_update(task_struct * task);

/// @brief Initialize the scheduler feedback system.
/// @return 1 on success, 0 on failure.
int scheduler_feedback_init();
