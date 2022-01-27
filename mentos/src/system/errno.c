/// @file errno.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/errno.h"
#include "process/scheduler.h"

/// @brief Returns the error number for the current process.
/// @return Pointer to the error number.
int *__geterrno()
{
    static int _errno            = 0;
    task_struct *current_process = scheduler_get_current_process();
    if (current_process == NULL) {
        return &_errno;
    }
    return &current_process->error_no;
}
