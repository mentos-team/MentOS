/// @file scheduler_feedback.h
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief 

#pragma once

#include "sys/types.h"

void writeFeedback(pid_t pid, char name[]);

/// @brief Initialize the scheduler feedback system.
/// @return 1 on success, 0 on failure.
int scheduler_feedback_init();
