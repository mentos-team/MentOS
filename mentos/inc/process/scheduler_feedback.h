/// @file scheduler_feedback.h
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief 

#pragma once

#include "sys/types.h"
#include "process/process.h"

/// @brief Function which is called by scheduler_algorithm after choosing the
/// next task, and updates the sceduling statistics.
void scheduler_feedback_update(task_struct * next);

/// @brief Initialize the scheduler feedback system.
/// @return 1 on success, 0 on failure.
int scheduler_feedback_init();
