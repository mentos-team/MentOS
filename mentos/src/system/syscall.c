/// @file syscall.c
/// @brief System Call management functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[SYSCLL]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "devices/fpu.h"
#include "mem/kheap.h"
#include "system/syscall.h"
#include "descriptor_tables/isr.h"
#include "sys/errno.h"
#include "kernel.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "sys/utsname.h"
#include "fs/ioctl.h"
#include "hardware/timer.h"

#include "ipc/msg.h"
#include "ipc/sem.h"
#include "ipc/shm.h"

/// The signature of a function call.
typedef int (*SystemCall)();

/// The list of function call.
SystemCall sys_call_table[SYSCALL_NUMBER];

/// Last interupt stack frame
static pt_regs *current_interrupt_stack_frame;

/// @brief A Not Implemented (NI) system-call.
/// @return Always returns -ENOSYS.
/// @details
/// Linux provides a "not implemented" system call, sys_ni_syscall(), which does
/// nothing except return ENOSYS, the error corresponding to an invalid
/// system call. This function is used to "plug the hole" in the rare event that
/// a syscall is removed or otherwise made unavailable.
static inline int sys_ni_syscall()
{
    return -ENOSYS;
}

void syscall_init()
{
    // Initialize the list of system calls.
    for (uint32_t it = 0; it < SYSCALL_NUMBER; ++it) {
        sys_call_table[it] = sys_ni_syscall;
    }

    sys_call_table[__NR_exit]           = (SystemCall)sys_exit;
    sys_call_table[__NR_read]           = (SystemCall)sys_read;
    sys_call_table[__NR_write]          = (SystemCall)sys_write;
    sys_call_table[__NR_open]           = (SystemCall)sys_open;
    sys_call_table[__NR_close]          = (SystemCall)sys_close;
    sys_call_table[__NR_stat]           = (SystemCall)sys_stat;
    sys_call_table[__NR_fstat]          = (SystemCall)sys_fstat;
    sys_call_table[__NR_mkdir]          = (SystemCall)sys_mkdir;
    sys_call_table[__NR_rmdir]          = (SystemCall)sys_rmdir;
    sys_call_table[__NR_creat]          = (SystemCall)sys_creat;
    sys_call_table[__NR_unlink]         = (SystemCall)sys_unlink;
    sys_call_table[__NR_getdents]       = (SystemCall)sys_getdents;
    sys_call_table[__NR_lseek]          = (SystemCall)sys_lseek;
    sys_call_table[__NR_getpid]         = (SystemCall)sys_getpid;
    sys_call_table[__NR_getsid]         = (SystemCall)sys_getsid;
    sys_call_table[__NR_setsid]         = (SystemCall)sys_setsid;
    sys_call_table[__NR_getpgid]        = (SystemCall)sys_getpgid;
    sys_call_table[__NR_setpgid]        = (SystemCall)sys_setpgid;
    sys_call_table[__NR_getuid]         = (SystemCall)sys_getuid;
    sys_call_table[__NR_setuid]         = (SystemCall)sys_setuid;
    sys_call_table[__NR_getgid]         = (SystemCall)sys_getgid;
    sys_call_table[__NR_setgid]         = (SystemCall)sys_setgid;
    sys_call_table[__NR_getppid]        = (SystemCall)sys_getppid;
    sys_call_table[__NR_sigaction]      = (SystemCall)sys_sigaction;
    sys_call_table[__NR_fork]           = (SystemCall)sys_fork;
    sys_call_table[__NR_execve]         = (SystemCall)sys_execve;
    sys_call_table[__NR_nice]           = (SystemCall)sys_nice;
    sys_call_table[__NR_kill]           = (SystemCall)sys_kill;
    sys_call_table[__NR_reboot]         = (SystemCall)sys_reboot;
    sys_call_table[__NR_uname]          = (SystemCall)sys_uname;
    sys_call_table[__NR_sigreturn]      = (SystemCall)sys_sigreturn;
    sys_call_table[__NR_waitpid]        = (SystemCall)sys_waitpid;
    sys_call_table[__NR_chdir]          = (SystemCall)sys_chdir;
    sys_call_table[__NR_fchdir]         = (SystemCall)sys_fchdir;
    sys_call_table[__NR_time]           = (SystemCall)sys_time;
    sys_call_table[__NR_sigprocmask]    = (SystemCall)sys_sigprocmask;
    sys_call_table[__NR_brk]            = (SystemCall)sys_brk;
    sys_call_table[__NR_signal]         = (SystemCall)sys_signal;
    sys_call_table[__NR_ioctl]          = (SystemCall)sys_ioctl;
    sys_call_table[__NR_sched_setparam] = (SystemCall)sys_sched_setparam;
    sys_call_table[__NR_sched_getparam] = (SystemCall)sys_sched_getparam;
    sys_call_table[__NR_nanosleep]      = (SystemCall)sys_nanosleep;
    sys_call_table[__NR_getcwd]         = (SystemCall)sys_getcwd;
    sys_call_table[__NR_waitperiod]     = (SystemCall)sys_waitperiod;
    sys_call_table[__NR_msgctl]         = (SystemCall)sys_msgctl;
    sys_call_table[__NR_msgget]         = (SystemCall)sys_msgget;
    sys_call_table[__NR_msgrcv]         = (SystemCall)sys_msgrcv;
    sys_call_table[__NR_msgsnd]         = (SystemCall)sys_msgsnd;
    sys_call_table[__NR_semctl]         = (SystemCall)sys_semctl;
    sys_call_table[__NR_semget]         = (SystemCall)sys_semget;
    sys_call_table[__NR_semop]          = (SystemCall)sys_semop;
    sys_call_table[__NR_shmat]          = (SystemCall)sys_shmat;
    sys_call_table[__NR_shmctl]         = (SystemCall)sys_shmctl;
    sys_call_table[__NR_shmdt]          = (SystemCall)sys_shmdt;
    sys_call_table[__NR_shmget]         = (SystemCall)sys_shmget;
    sys_call_table[__NR_alarm]          = (SystemCall)sys_alarm;
    sys_call_table[__NR_setitimer]      = (SystemCall)sys_setitimer;
    sys_call_table[__NR_getitimer]      = (SystemCall)sys_getitimer;

    isr_install_handler(SYSTEM_CALL, &syscall_handler, "syscall_handler");
}

pt_regs *get_current_interrupt_stack_frame()
{
    return current_interrupt_stack_frame;
}

void syscall_handler(pt_regs *f)
{
    // Saves current interrupt stack frame
    current_interrupt_stack_frame = f;

    // dbg_print_regs(f);
    // Save current process fpu state.
    switch_fpu();

    // The index of the requested system call.
    uint32_t sc_index = f->eax;

    // The result of the system call.
    int ret;
    if (sc_index >= SYSCALL_NUMBER) {
        ret = ENOSYS;
    } else {
        uintptr_t ptr = (uintptr_t)sys_call_table[sc_index];

        SystemCall func = (SystemCall)ptr;

        uint32_t arg0 = f->ebx;
        uint32_t arg1 = f->ecx;
        uint32_t arg2 = f->edx;
        uint32_t arg3 = f->esi;
        uint32_t arg4 = f->edi;
        if ((sc_index == __NR_fork) ||
            (sc_index == __NR_clone) ||
            (sc_index == __NR_execve) ||
            (sc_index == __NR_sigreturn)) {
            arg0 = (uintptr_t)f;
        }
        ret = func(arg0, arg1, arg2, arg3, arg4);
    }
    f->eax = ret;

    // Schedule next process.
    scheduler_run(f);
    // Restore fpu state.
    unswitch_fpu();
}
