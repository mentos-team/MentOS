///                MentOS, The Mentoring Operating system project
/// @file syscall.c
/// @brief System Call management functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "syscall.h"
#include "sys.h"
#include "shm.h"
#include "isr.h"
#include "errno.h"
#include "video.h"
#include "fcntl.h"
#include "kernel.h"
#include "unistd.h"
#include "process.h"
#include "process.h"
#include "irqflags.h"
#include "scheduler.h"
#include "read_write.h"

/// @brief The signature of a function call.
typedef int (*SystemCall)();

/// @brief The signature used to call the system call.
typedef uint32_t (*SystemCallFun)(uint32_t, ...);

/// @brief The list of function call.
SystemCall sys_call_table[SYSCALL_NUMBER];

// Linux provides a "not implemented" system call, sys_ni_syscall(), which does
// nothing except return ENOSYS, the error corresponding to an invalid
// system call. This function is used to "plug the hole" in the rare event that
// a syscall is removed or otherwise made unavailable.
int sys_ni_syscall()
{
	return ENOSYS;
}

void syscall_init()
{
	// Initialize the list of system calls.
	for (uint32_t it = 0; it < SYSCALL_NUMBER; ++it) {
		sys_call_table[it] = sys_ni_syscall;
	}

	sys_call_table[__NR_exit] = (SystemCall)sys_exit;
	sys_call_table[__NR_read] = (SystemCall)sys_read;
	sys_call_table[__NR_write] = (SystemCall)sys_write;
	sys_call_table[__NR_open] = (SystemCall)sys_open;
	sys_call_table[__NR_getpid] = (SystemCall)sys_getpid;
	sys_call_table[__NR_getppid] = (SystemCall)sys_getppid;
	sys_call_table[__NR_vfork] = (SystemCall)sys_vfork;
	sys_call_table[__NR_execve] = (SystemCall)sys_execve;
	sys_call_table[__NR_nice] = (SystemCall)sys_nice;
	sys_call_table[__NR_reboot] = (SystemCall)sys_reboot;
	sys_call_table[__NR_waitpid] = (SystemCall)sys_waitpid;
	sys_call_table[__NR_chdir] = (SystemCall)sys_chdir;
	sys_call_table[__NR_getcwd] = (SystemCall)sys_getcwd;
	sys_call_table[__NR_waitpid] = (SystemCall)sys_waitpid;
	sys_call_table[__NR_brk] = (SystemCall)umalloc; // TODO: sys_brk
	sys_call_table[__NR_free] = (SystemCall)ufree; // TODO: sys_brk

	isr_install_handler(SYSTEM_CALL, &syscall_handler, "syscall_handler");
}

void syscall_handler(pt_regs *f)
{
	// print_intrframe(f);

	// The index of the requested system call.
	uint32_t sc_index = f->eax;

	// The result of the system call.
	int ret;
	if (sc_index >= SYSCALL_NUMBER) {
		ret = ENOSYS;
	} else {
		uintptr_t ptr = (uintptr_t)sys_call_table[sc_index];

		SystemCallFun func = (SystemCallFun)ptr;

		uint32_t arg0 = f->ebx;
		uint32_t arg1 = f->ecx;
		uint32_t arg2 = f->edx;
		uint32_t arg3 = f->esi;
		uint32_t arg4 = f->edi;
		if ((sc_index == __NR_vfork) || (sc_index == __NR_clone)) {
			arg0 = (uintptr_t)f;
		} else if (sc_index == __NR_execve) {
			arg0 = (uintptr_t)f;
		}
		ret = func(arg0, arg1, arg2, arg3, arg4);
	}
	f->eax = ret;

	// Schedule next process.
	kernel_schedule(f);
}
