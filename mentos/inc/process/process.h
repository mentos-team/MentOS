///                MentOS, The Mentoring Operating system project
/// @file process.h
/// @brief Process data structures and functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list.h"
#include "tree.h"
#include "clock.h"
#include "types.h"
#include "stdint.h"
#include "stddef.h"
#include "paging.h"
#include "kernel.h"
#include "unistd.h"
#include "list_head.h"
#include "signal_defs.h"

/// The maximum length of a name for a task_struct.
#define TASK_NAME_MAX_LENGTH 100

/// The default dimension of the stack of a process (1 MByte).
#define DEFAULT_STACK_SIZE 0x100000

//==== Task state ==============================================================
// Used in tsk->state
/// The task is running.
#define TASK_RUNNING 0x0000

/// The task is interruptible.
#define TASK_INTERRUPTIBLE 0x0001

/// The task is uninterruptible.
#define TASK_UNINTERRUPTIBLE 0x0002

// Used in tsk->exit_state
/// The task is dead.
#define EXIT_DEAD 0x0010

/// The task is zombie.
#define EXIT_ZOMBIE 0x0020
//==============================================================================

/// @brief Every task_struck has a sched_entity. This structure is used to track
/// the statistics of a process. While the other variables also play a role in
/// CFS decisions'algorithm, vruntime is by far the core variable which needs
/// more attention as to understand the scheduling decision process.
/// @details
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

	/// Overall execution time.
	time_t sum_exec_runtime;

	/// weighted execution time.
	time_t vruntime;

} sched_entity;

// x86 thread (x86 and FPU registers).
typedef struct thread_struct {
	/// 32-bit base pointer register.
	uint32_t ebp;

	/// 32-bit stack pointer register.
	uint32_t useresp;

	/// 32-bit base register.
	uint32_t ebx;

	/// 32-bit data register.
	uint32_t edx;

	/// 32-bit counter.
	uint32_t ecx;

	/// 32-bit accumulator register.
	uint32_t eax;

	/// Instruction Pointer Register.
	uint32_t eip;

	/// 32-bit flag register.
	uint32_t eflags;

	/// FS and GS have no hardware-assigned uses.
	uint32_t gs;

	/// FS and GS have no hardware-assigned uses.
	uint32_t fs;

	/// Extra Segment determined by the programmer.
	uint32_t es;

	/// Data Segment.
	uint32_t ds;

	/// 32-bit destination register.
	uint32_t edi;

	/// 32-bit source register.
	uint32_t esi;

	// ///< Code Segment.
	// uint32_t cs;

	// ///< Stack Segment.
	// uint32_t ss;

	/// Determines if the FPU is enabled.
	bool_t fpu_enabled;

	/// Data structure used to save FPU registers.
	savefpu fpu_register;
} thread_struct;

/// @brief this is our task object. Every process in the system has this, and
/// it holds a lot of information. It’ll hold mm information, it’s name,
/// statistics, etc..
typedef struct task_struct {
	/// The pid of the process.
	pid_t pid;

	// -1 unrunnable, 0 runnable, >0 stopped.
	/// The current state of the process:
	__volatile__ long state;

	/// Pointer to process's parent.
	struct task_struct *parent;

	/// List head for scheduling purposes.
	list_head run_list;

	/// List of children traced by the process.
	list_head children;

	/// List of siblings.
	list_head sibling;

	/// The context of the processors.
	thread_struct thread;

	/// For scheduling algorithms.
	sched_entity se;

	/// Exit code of the process. (parameter of _exit() system call).
	int exit_code;

	/// The name of the task (Added for debug purpose).
	char name[TASK_NAME_MAX_LENGTH];

	/// Task's segments.
	struct mm_struct *mm;

	/// Task's specific error number.
	int error_no;

	/// The current working directory.
	char cwd[MAX_PATH_LENGTH];

	//==== Future work =========================================================
	// - task's attributes:
	// struct task_struct __rcu	*real_parent;
	// int exit_state;
	// int exit_signal;
	// struct thread_info thread_info;
	// List of sibling, namely processes created by parent process
	// struct list_head sibling;

	// - task's file descriptor:
	// struct files_struct *files;

	// - task's signal handlers:
	// struct signal_struct	*signal;
	// struct sighand_struct *sighand;
	// sigset_t	blocked;
	// sigset_t	real_blocked;
	// sigset_t	saved_sigmask;
	// struct sigpending pending;
	//==========================================================================

} task_struct;

/// @brief Create a child process.
pid_t sys_vfork(pt_regs *r);

// TODO: doxygen comment.
char *get_current_dir_name();

// TODO: doxygen comment.
void sys_getcwd(char *path, size_t size);

// TODO: doxygen comment.s
void sys_chdir(char const *path);

/// @brief Replaces the current process image with a new process image.
int sys_execve(pt_regs *r);

/// @brief Create and spawn the init process.
struct task_struct *create_init_process();
