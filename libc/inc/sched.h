///                MentOS, The Mentoring Operating system project
/// @file sched.h
/// @brief Structures and functions for managing the scheduler.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/types.h"
#include "time.h"
#include "stdbool.h"

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

int sched_setparam(pid_t pid, const sched_param_t *param);

int sched_getparam(pid_t pid, sched_param_t *param);

int waitperiod();
