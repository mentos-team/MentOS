///                MentOS, The Mentoring Operating system project
/// @file errno.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "scheduler.h"

int *__geterrno()
{
    static int _errno = 0;

    task_struct *current_process = kernel_get_current_process();

    if (current_process == NULL)
    {
        return &_errno;
    }

    return &current_process->error_no;
}
