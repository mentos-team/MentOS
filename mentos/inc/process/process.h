/// @file process.h
/// @brief Process data structures and functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "drivers/keyboard/keyboard.h"
#include "bits/termios-struct.h"
#include "system/signal.h"
#include "devices/fpu.h"
#include "mem/paging.h"
#include "stdbool.h"

/// The maximum length of a name for a task_struct.
#define TASK_NAME_MAX_LENGTH 100

/// The default dimension of the stack of a process (1 MByte).
#define DEFAULT_STACK_SIZE (1 * M)

/// @brief This structure is used to track the statistics of a process.
/// @details
/// While the other variables also play a role in
/// CFS decisions'algorithm, vruntime is by far the core variable which needs
/// more attention as to understand the scheduling decision process.
///
/// The nice value is a user-space and priority 'prio' is the process's actual
/// priority that use by Linux kernel. In linux system priorities are 0 to 139
/// in which 0 to 99 for real time and 100 to 139 for users.
/// The nice value range is -20 to +19 where -20 is highest, 0 default and +19
/// is lowest. relation between nice value and priority is : PR = 20 + NI.
typedef struct sched_entity_t {
    /// Static execution priority.
    int prio;

    /// Start execution time.
    time_t start_runtime;
    /// Last context switch time.
    time_t exec_start;
    /// Last execution time.
    time_t exec_runtime;
    /// Overall execution time.
    time_t sum_exec_runtime;
    /// Weighted execution time.
    time_t vruntime;

    /// Expected period of the task
    time_t period;
    /// Absolute deadline
    time_t deadline;
    /// Absolute time of arrival of the task
    time_t arrivaltime;
    /// Has already executed
    bool_t executed;
    /// Determines if it is a periodic task.
    bool_t is_periodic;
    /// Determines if we need to analyze the WCET of the process.
    bool_t is_under_analysis;
    /// Beginning of next period
    time_t next_period;
    /// Worst case execution time
    time_t worst_case_exec;
    /// Processor utilization factor
    double utilization_factor;
} sched_entity_t;

/// @brief Stores the status of CPU and FPU registers.
typedef struct thread_struct_t {
    /// Stored status of registers.
    pt_regs regs;
    /// Stored status of registers befor jumping into a signal handler.
    pt_regs signal_regs;
    /// Determines if the FPU is enabled.
    bool_t fpu_enabled;
    /// Data structure used to save FPU registers.
    savefpu fpu_register;
} thread_struct_t;

/// @brief this is our task object. Every process in the system has this, and
/// it holds a lot of information. It’ll hold mm information, it’s name,
/// statistics, etc..
typedef struct task_struct {
    /// The pid of the process.
    pid_t pid;
    /// The session id of the process
    pid_t sid;
    /// The Process Group Id of the process
    pid_t pgid;
    /// The Group ID (GID) of the process
    pid_t rgid;
    /// The effective Group ID (GID) of the process
    pid_t gid;
    /// The User ID (UID) of the user owning the process.
    uid_t ruid;
    /// The effective User ID (UID) of the process.
    uid_t uid;
    // -1 unrunnable, 0 runnable, >0 stopped.
    /// The current state of the process:
    __volatile__ long state;
    /// The current opened file descriptors
    vfs_file_descriptor_t *fd_list;
    /// The maximum supported number of file descriptors
    int max_fd;
    /// Pointer to process's parent.
    struct task_struct *parent;
    /// List head for scheduling purposes.
    list_head run_list;
    /// List of children traced by the process.
    list_head children;
    /// List of siblings, namely processes created by parent process.
    list_head sibling;
    /// The context of the processors.
    thread_struct_t thread;
    /// For scheduling algorithms.
    sched_entity_t se;
    /// Exit code of the process. (parameter of _exit() system call).
    int exit_code;
    /// The name of the task (Added for debug purpose).
    char name[TASK_NAME_MAX_LENGTH];
    /// Task's segments.
    mm_struct_t *mm;
    /// Task's specific error number.
    int error_no;
    /// The current working directory.
    char cwd[PATH_MAX];

    /// Address of the LIBC sigreturn function.
    uint32_t sigreturn_addr;
    /// Pointer to the process’s signal handler descriptor
    sighand_t sighand;
    /// Mask of blocked signals.
    sigset_t blocked;
    /// Temporary mask of blocked signals (used by the rt_sigtimedwait() system call)
    sigset_t real_blocked;
    /// The previous sig mask.
    sigset_t saved_sigmask;
    /// Data structure storing the private pending signals
    sigpending_t pending;

    /// Timer for alarm syscall.
    struct timer_list *real_timer;

    /// Next value for the real timer (ITIMER_REAL).
    unsigned long it_real_incr;
    /// Current value for the real timer (ITIMER_REAL).
    unsigned long it_real_value;
    /// Next value for the virtual timer (ITIMER_VIRTUAL).
    unsigned long it_virt_incr;
    /// Current value for the virtual timer (ITIMER_VIRTUAL).
    unsigned long it_virt_value;
    /// Next value for the profiling timer (ITIMER_PROF).
    unsigned long it_prof_incr;
    /// Current value for the profiling timer (ITIMER_PROF).
    unsigned long it_prof_value;

    /// Process-wise terminal options.
    termios_t termios;
    /// Buffer for managing inputs from keyboard.
    fs_rb_scancode_t keyboard_rb;

    //==== Future work =========================================================
    // - task's attributes:
    // struct task_struct __rcu	*real_parent;
    // int exit_state;
    // int exit_signal;
    // struct thread_info thread_info;
    //==========================================================================
} task_struct;

/// @brief Initialize the task management.
/// @return 1 success, 0 failure.
int init_tasking(void);

/// @brief Create and spawn the init process.
/// @param path Path of the `init` program.
/// @return Pointer to init process.
task_struct *process_create_init(const char *path);
