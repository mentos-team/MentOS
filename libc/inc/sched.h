/// @file sched.h
/// @brief Structures and functions for managing the scheduler.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
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

/// @brief Sets scheduling parameters.
/// @param pid pid of the process we want to change the parameters. If zero,
/// then the parameters of the calling process are set.
/// @param param The interpretation of the argument param depends on the
/// scheduling policy of the thread identified by pid.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
int sched_setparam(pid_t pid, const sched_param_t *param);

/// @brief Gets scheduling parameters.
/// @param pid pid of the process we want to retrieve the parameters. If zero,
/// then the parameters of the calling process are returned.
/// @param param The interpretation of the argument param depends on the
/// scheduling policy of the thread identified by pid.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
int sched_getparam(pid_t pid, sched_param_t *param);

/// @brief Placed at the end of an infinite while loop, stops the process until,
/// its next period starts. The calling process must be a periodic one.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
int waitperiod();
