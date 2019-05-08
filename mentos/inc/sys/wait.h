///                MentOS, The Mentoring Operating system project
/// @file wait.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "types.h"

/// @brief Return immediately if no child is there to be waited for.
#define WNOHANG 0x00000001

/// @brief Return for children that are stopped, and whose status has not
///        been reported.
#define WUNTRACED 0x00000002

/// @brief returns true if the child process exited because of a signal that
///        was not caught.
#define WIFSIGNALED(status) (!WIFSTOPPED(status) && !WIFEXITED(status))

/// @brief returns true if the child process that caused the return is
///        currently stopped; this is only possible if the call was done using
///        WUNTRACED().
#define WIFSTOPPED(status) (((status)&0xff) == 0x7f)

/// @brief evaluates to the least significant eight bits of the return code
///        of the child that terminated, which may have been set as the argument
///        to a call to exit() or as the argument for a return statement in the
///        main  program. This macro can only be evaluated if WIFEXITED()
///        returned nonzero.
#define WEXITSTATUS(status) (((status)&0xff00) >> 8)

/// @brief returns the number of the signal that caused the child process to
///        terminate. This macro can only be evaluated if WIFSIGNALED() returned
///        nonzero.
#define WTERMSIG(status) ((status)&0x7f)

/// @brief Is nonzero if the child exited normally.
#define WIFEXITED(status) (WTERMSIG(status) == 0)

/// @brief returns the number of the signal that caused the child to stop.
///        This macro can only be evaluated if WIFSTOPPED() returned nonzero.
#define WSTOPSIG(status) (WEXITSTATUS(status))

extern pid_t wait(int *status);

/// @brief   Suspends the execution of the calling thread until a child
///          specified by pid argument has changed state.
/// @details
///          By default, waitpid() waits only for terminated children, but this
///          behavior is modifiable via the options argument, as described below.
///          The value of pid can be:
///        - 1    meaning wait for any child process.
///          0    meaning wait for any child process whose process group ID is
///               equal to that of the calling process.
///        > 0    meaning wait for the child whose process ID is equal to the
///               value of pid.
/// @return On error, -1 is returned.
extern pid_t waitpid(pid_t pid, int *status, int options);
