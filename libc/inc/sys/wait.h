/// @file wait.h
/// @brief Event management functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

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

//==== Task States ============================================================
#define TASK_RUNNING         0x00     ///< The process is either: 1) running on CPU or 2) waiting in a run queue.
#define TASK_INTERRUPTIBLE   (1 << 0) ///< The process is sleeping, waiting for some event to occur.
#define TASK_UNINTERRUPTIBLE (1 << 1) ///< Similar to TASK_INTERRUPTIBLE, but it doesn't process signals.
#define TASK_STOPPED         (1 << 2) ///< Stopped, it's not running, and not able to run.
#define TASK_TRACED          (1 << 3) ///< Is being monitored by other processes such as debuggers.
#define EXIT_ZOMBIE          (1 << 4) ///< The process has terminated.
#define EXIT_DEAD            (1 << 5) ///< The final state.
//==============================================================================

/// @brief Suspends the execution of the calling thread until ANY child has
///        changed state.
/// @param status Variable where the new status of the child is stored.
/// @return On error, -1 is returned, otherwise it returns the pid of the
///         child that has unlocked the wait.
extern pid_t wait(int *status);

/// @brief   Suspends the execution of the calling thread until a child
///          specified by pid argument has changed state.
/// @param pid     Se details below for more information.
/// @param status  Variable where the new status of the child is stored.
/// @param options Waitpid options.
/// @return On error, -1 is returned, otherwise it returns the pid of the
///         child that has unlocked the wait.
/// @details
///          By default, waitpid() waits only for terminated children, but this
///          behavior is modifiable via the options argument, as described below.
///          The value of pid can be:
///        - 1    meaning wait for any child process.
///          0    meaning wait for any child process whose process group ID is
///               equal to that of the calling process.
///        > 0    meaning wait for the child whose process ID is equal to the
///               value of pid.
extern pid_t waitpid(pid_t pid, int *status, int options);
