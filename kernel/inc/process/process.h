/// @file process.h
/// @brief Process data structures and functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "bits/termios-struct.h"
#include "devices/fpu.h"
#include "drivers/keyboard/keyboard.h"
#include "limits.h"
#include "mem/paging.h"
#include "stdbool.h"
#include "system/signal.h"

/// @brief The kernel-internal cwd query, without the syscall pointer gate:
/// the caller passes a kernel buffer, which must not be validated against
/// the current task's user memory (#191).
/// @param buf the kernel buffer receiving the cwd.
/// @param size the size of the buffer.
/// @return The buffer, or a negative value on error.
char *do_getcwd(char *buf, size_t size);

/// The maximum length of a name for a task_struct.
/// NAME_MAX is the same bound sys_execve already applies to the argv[0] copy
/// buffer, so a name that survives execve always fits this field.
#define TASK_NAME_MAX_LENGTH NAME_MAX

/// The default dimension of the stack of a process (1 MByte).
#define DEFAULT_STACK_SIZE (1 * M)

/// The private kernel continuation stack budget for each task.
///
/// This is deliberately separate from the user stack in the task's mm. The
/// stack is used for kernel entry/return state and grows downward.
#define TASK_KERNEL_STACK_SIZE (128 * K)
/// Order of the 32-page private kernel stack allocation.
#define TASK_KERNEL_STACK_ORDER 5
/// Bytes reserved at the low end for an overflow canary.
#define TASK_KERNEL_STACK_CANARY_SIZE 16
/// Value used to detect writes below the continuation stack.
#define TASK_KERNEL_STACK_CANARY 0xC0DEC0DEu
/// Fill value used to measure the high-water mark of a kernel stack.
#define TASK_KERNEL_STACK_FILL 0xA5A5A5A5u

/* Keep the allocator order and the advertised usable extent in lock-step. */
#if TASK_KERNEL_STACK_SIZE != ((1U << TASK_KERNEL_STACK_ORDER) * PAGE_SIZE)
#error "TASK_KERNEL_STACK_SIZE must match TASK_KERNEL_STACK_ORDER"
#endif

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
typedef struct sched_entity {
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
typedef struct thread_struct {
    /// Stored status of registers.
    pt_regs_t regs;
    /// Live outer user frame while this task is executing at the boundary.
    /// This pointer is transient and is never inherited by fork.
    pt_regs_t *user_regs;
    /// Saved ESP of the task's resumable kernel continuation.
    uint32_t kernel_esp;
    /// Stored status of registers befor jumping into a signal handler.
    pt_regs_t signal_regs;
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
    gid_t rgid;
    /// The effective Group ID (GID) of the process
    gid_t gid;
    /// The User ID (UID) of the user owning the process.
    uid_t ruid;
    /// The effective User ID (UID) of the process.
    uid_t uid;
    // -1 unrunnable, 0 runnable, >0 stopped.
    /// The current state of the process:
    __volatile__ long state;
    /// Set by asynchronous events; consumed by the common return boundary.
    __volatile__ bool_t need_resched;
    /// The current opened file descriptors
    vfs_file_descriptor_t *fd_list;
    /// The maximum supported number of file descriptors
    int max_fd;
    /// Pointer to process's parent.
    struct task_struct *parent;
    /// List head for scheduling purposes.
    list_head_t run_list;
    /// List of children traced by the process.
    list_head_t children;
    /// List of siblings, namely processes created by parent process.
    list_head_t sibling;
    /// The context of the processors.
    thread_struct_t thread;
    /// Private kernel continuation stack storage owned by this task.
    void *kernel_stack;
    /// One-past-the-end address used as the TSS esp0 value on kernel entry.
    uintptr_t kernel_stack_top;
    /// Size of the private kernel continuation stack in bytes.
    size_t kernel_stack_size;
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
    /// Timer for nanosleep syscall (cancelled on signal delivery).
    struct timer_list *sleep_timer;

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
    rb_keybuffer_t keyboard_rb;

    /// Wait queue this task is currently sleeping on (NULL if not sleeping).
    struct wait_queue_head *waiting_on;

    //==== Future work =========================================================
    // - task's attributes:
    // struct task_struct __rcu	*real_parent;
    // int exit_state;
    // int exit_signal;
    // struct thread_info thread_info;
    //==========================================================================
} task_struct;

/// @brief Acquire the private kernel continuation stack for a task.
/// @return 1 on success, 0 when allocation fails or task is NULL.
int task_kernel_stack_alloc(task_struct *task);

/// @brief Release a task's private kernel continuation stack.
void task_kernel_stack_free(task_struct *task);

/// @brief Check the low-address canary of a task's private stack.
/// @return 1 when intact or no stack is allocated, 0 on corruption.
int task_kernel_stack_check(const task_struct *task);

/// @brief Return the deepest observed stack usage in bytes.
/// @details The value is a diagnostic lower bound, not a proof of safety.
size_t task_kernel_stack_watermark(const task_struct *task);

/// @brief Build an inactive first-return frame from the task's user snapshot.
/// @return 0 on success, -1 if the task has no private stack.
int task_prepare_kernel_context(task_struct *task);

/// @brief Initialize the task management.
/// @return 1 success, 0 failure.
int init_tasking(void);

/// @brief Create and spawn the init process.
/// @param path Path of the `init` program.
/// @return 0 on success, 1 on failure.
int process_create_init(const char *path);

/// @brief Get a file structure from a file descriptor.
/// @param fd the file descriptor.
/// @return Returns the file structure corresponding to the given file
/// descriptor or NULL if the file descriptor is invalid or the file has been
/// closed.
vfs_file_descriptor_t *fget(int fd);
