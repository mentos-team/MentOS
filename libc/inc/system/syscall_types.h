/// @file syscall_types.h
/// @brief System Call numbers.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#define __NR_exit                   1   ///< System-call number for `exit`
#define __NR_fork                   2   ///< System-call number for `fork`
#define __NR_read                   3   ///< System-call number for `read`
#define __NR_write                  4   ///< System-call number for `write`
#define __NR_open                   5   ///< System-call number for `open`
#define __NR_close                  6   ///< System-call number for `close`
#define __NR_waitpid                7   ///< System-call number for `waitpid`
#define __NR_creat                  8   ///< System-call number for `creat`
#define __NR_link                   9   ///< System-call number for `link`
#define __NR_unlink                 10  ///< System-call number for `unlink`
#define __NR_execve                 11  ///< System-call number for `execve`
#define __NR_chdir                  12  ///< System-call number for `chdir`
#define __NR_time                   13  ///< System-call number for `time`
#define __NR_mknod                  14  ///< System-call number for `mknod`
#define __NR_chmod                  15  ///< System-call number for `chmod`
#define __NR_lchown                 16  ///< System-call number for `lchown`
#define __NR_stat                   18  ///< System-call number for `stat`
#define __NR_lseek                  19  ///< System-call number for `lseek`
#define __NR_getpid                 20  ///< System-call number for `getpid`
#define __NR_mount                  21  ///< System-call number for `mount`
#define __NR_oldumount              22  ///< System-call number for `oldumount`
#define __NR_setuid                 23  ///< System-call number for `setuid`
#define __NR_getuid                 24  ///< System-call number for `getuid`
#define __NR_stime                  25  ///< System-call number for `stime`
#define __NR_ptrace                 26  ///< System-call number for `ptrace`
#define __NR_alarm                  27  ///< System-call number for `alarm`
#define __NR_fstat                  28  ///< System-call number for `fstat`
#define __NR_pause                  29  ///< System-call number for `pause`
#define __NR_utime                  30  ///< System-call number for `utime`
#define __NR_access                 33  ///< System-call number for `access`
#define __NR_nice                   34  ///< System-call number for `nice`
#define __NR_sync                   36  ///< System-call number for `sync`
#define __NR_kill                   37  ///< System-call number for `kill`
#define __NR_rename                 38  ///< System-call number for `rename`
#define __NR_mkdir                  39  ///< System-call number for `mkdir`
#define __NR_rmdir                  40  ///< System-call number for `rmdir`
#define __NR_dup                    41  ///< System-call number for `dup`
#define __NR_pipe                   42  ///< System-call number for `pipe`
#define __NR_times                  43  ///< System-call number for `times`
#define __NR_brk                    45  ///< System-call number for `brk`
#define __NR_setgid                 46  ///< System-call number for `setgid`
#define __NR_getgid                 47  ///< System-call number for `getgid`
#define __NR_signal                 48  ///< System-call number for `signal`
#define __NR_geteuid                49  ///< System-call number for `geteuid`
#define __NR_getegid                50  ///< System-call number for `getegid`
#define __NR_acct                   51  ///< System-call number for `acct`
#define __NR_umount                 52  ///< System-call number for `umount`
#define __NR_ioctl                  54  ///< System-call number for `ioctl`
#define __NR_fcntl                  55  ///< System-call number for `fcntl`
#define __NR_setpgid                57  ///< System-call number for `setpgid`
#define __NR_olduname               59  ///< System-call number for `olduname`
#define __NR_umask                  60  ///< System-call number for `umask`
#define __NR_chroot                 61  ///< System-call number for `chroot`
#define __NR_ustat                  62  ///< System-call number for `ustat`
#define __NR_dup2                   63  ///< System-call number for `dup2`
#define __NR_getppid                64  ///< System-call number for `getppid`
#define __NR_getpgrp                65  ///< System-call number for `getpgrp`
#define __NR_setsid                 66  ///< System-call number for `setsid`
#define __NR_sigaction              67  ///< System-call number for `sigaction`
#define __NR_sgetmask               68  ///< System-call number for `sgetmask`
#define __NR_ssetmask               69  ///< System-call number for `ssetmask`
#define __NR_setreuid               70  ///< System-call number for `setreuid`
#define __NR_setregid               71  ///< System-call number for `setregid`
#define __NR_sigsuspend             72  ///< System-call number for `sigsuspend`
#define __NR_sigpending             73  ///< System-call number for `sigpending`
#define __NR_sethostname            74  ///< System-call number for `sethostname`
#define __NR_setrlimit              75  ///< System-call number for `setrlimit`
#define __NR_getrlimit              76  ///< System-call number for `getrlimit`
#define __NR_getrusage              77  ///< System-call number for `getrusage`
#define __NR_gettimeofday           78  ///< System-call number for `gettimeofday`
#define __NR_settimeofday           79  ///< System-call number for `settimeofday`
#define __NR_getgroups              80  ///< System-call number for `getgroups`
#define __NR_setgroups              81  ///< System-call number for `setgroups`
#define __NR_symlink                83  ///< System-call number for `symlink`
#define __NR_lstat                  84  ///< System-call number for `lstat`
#define __NR_readlink               85  ///< System-call number for `readlink`
#define __NR_uselib                 86  ///< System-call number for `uselib`
#define __NR_swapon                 87  ///< System-call number for `swapon`
#define __NR_reboot                 88  ///< System-call number for `reboot`
#define __NR_readdir                89  ///< System-call number for `readdir`
#define __NR_mmap                   90  ///< System-call number for `mmap`
#define __NR_munmap                 91  ///< System-call number for `munmap`
#define __NR_truncate               92  ///< System-call number for `truncate`
#define __NR_ftruncate              93  ///< System-call number for `ftruncate`
#define __NR_fchmod                 94  ///< System-call number for `fchmod`
#define __NR_fchown                 95  ///< System-call number for `fchown`
#define __NR_getpriority            96  ///< System-call number for `getpriority`
#define __NR_setpriority            97  ///< System-call number for `setpriority`
#define __NR_statfs                 99  ///< System-call number for `statfs`
#define __NR_fstatfs                100 ///< System-call number for `fstatfs`
#define __NR_ioperm                 101 ///< System-call number for `ioperm`
#define __NR_socketcall             102 ///< System-call number for `socketcall`
#define __NR_syslog                 103 ///< System-call number for `syslog`
#define __NR_setitimer              104 ///< System-call number for `setitimer`
#define __NR_getitimer              105 ///< System-call number for `getitimer`
#define __NR_newstat                106 ///< System-call number for `newstat`
#define __NR_newlstat               107 ///< System-call number for `newlstat`
#define __NR_newfstat               108 ///< System-call number for `newfstat`
#define __NR_uname                  109 ///< System-call number for `uname`
#define __NR_iopl                   110 ///< System-call number for `iopl`
#define __NR_vhangup                111 ///< System-call number for `vhangup`
#define __NR_idle                   112 ///< System-call number for `idle`
#define __NR_vm86old                113 ///< System-call number for `vm86old`
#define __NR_wait4                  114 ///< System-call number for `wait4`
#define __NR_swapoff                115 ///< System-call number for `swapoff`
#define __NR_sysinfo                116 ///< System-call number for `sysinfo`
#define __NR_ipc                    117 ///< System-call number for `ipc`
#define __NR_fsync                  118 ///< System-call number for `fsync`
#define __NR_sigreturn              119 ///< System-call number for `sigreturn`
#define __NR_clone                  120 ///< System-call number for `clone`
#define __NR_setdomainname          121 ///< System-call number for `setdomainname`
#define __NR_newuname               122 ///< System-call number for `newuname`
#define __NR_modify_ldt             123 ///< System-call number for `modify_ldt`
#define __NR_adjtimex               124 ///< System-call number for `adjtimex`
#define __NR_mprotect               125 ///< System-call number for `mprotect`
#define __NR_sigprocmask            126 ///< System-call number for `sigprocmask`
#define __NR_create_module          127 ///< System-call number for `create_module`
#define __NR_init_module            128 ///< System-call number for `init_module`
#define __NR_delete_module          129 ///< System-call number for `delete_module`
#define __NR_get_kernel_syms        130 ///< System-call number for `get_kernel_syms`
#define __NR_quotactl               131 ///< System-call number for `quotactl`
#define __NR_getpgid                132 ///< System-call number for `getpgid`
#define __NR_fchdir                 133 ///< System-call number for `fchdir`
#define __NR_bdflush                134 ///< System-call number for `bdflush`
#define __NR_sysfs                  135 ///< System-call number for `sysfs`
#define __NR_personality            136 ///< System-call number for `personality`
#define __NR_setfsuid               138 ///< System-call number for `setfsuid`
#define __NR_setfsgid               139 ///< System-call number for `setfsgid`
#define __NR_llseek                 140 ///< System-call number for `llseek`
#define __NR_getdents               141 ///< System-call number for `getdents`
#define __NR_select                 142 ///< System-call number for `select`
#define __NR_flock                  143 ///< System-call number for `flock`
#define __NR_msync                  144 ///< System-call number for `msync`
#define __NR_readv                  145 ///< System-call number for `readv`
#define __NR_writev                 146 ///< System-call number for `writev`
#define __NR_getsid                 147 ///< System-call number for `getsid`
#define __NR_fdatasync              148 ///< System-call number for `fdatasync`
#define __NR_sysctl                 149 ///< System-call number for `sysctl`
#define __NR_mlock                  150 ///< System-call number for `mlock`
#define __NR_munlock                151 ///< System-call number for `munlock`
#define __NR_mlockall               152 ///< System-call number for `mlockall`
#define __NR_munlockall             153 ///< System-call number for `munlockall`
#define __NR_sched_setparam         154 ///< System-call number for `sched_setparam`
#define __NR_sched_getparam         155 ///< System-call number for `sched_getparam`
#define __NR_sched_setscheduler     156 ///< System-call number for `sched_setscheduler`
#define __NR_sched_getscheduler     157 ///< System-call number for `sched_getscheduler`
#define __NR_sched_yield            158 ///< System-call number for `sched_yield`
#define __NR_sched_get_priority_max 159 ///< System-call number for `sched_get_priority_max`
#define __NR_sched_get_priority_min 160 ///< System-call number for `sched_get_priority_min`
#define __NR_sched_rr_get_interval  161 ///< System-call number for `sched_rr_get_interval`
#define __NR_nanosleep              162 ///< System-call number for `nanosleep`
#define __NR_mremap                 163 ///< System-call number for `mremap`
#define __NR_setresuid              164 ///< System-call number for `setresuid`
#define __NR_getresuid              165 ///< System-call number for `getresuid`
#define __NR_vm86                   166 ///< System-call number for `vm86`
#define __NR_query_module           167 ///< System-call number for `query_module`
#define __NR_poll                   168 ///< System-call number for `poll`
#define __NR_nfsservctl             169 ///< System-call number for `nfsservctl`
#define __NR_setresgid              170 ///< System-call number for `setresgid`
#define __NR_getresgid              171 ///< System-call number for `getresgid`
#define __NR_prctl                  172 ///< System-call number for `prctl`
#define __NR_rt_sigreturn           173 ///< System-call number for `rt_sigreturn`
#define __NR_rt_sigaction           174 ///< System-call number for `rt_sigaction`
#define __NR_rt_sigprocmask         175 ///< System-call number for `rt_sigprocmask`
#define __NR_rt_sigpending          176 ///< System-call number for `rt_sigpending`
#define __NR_rt_sigtimedwait        177 ///< System-call number for `rt_sigtimedwait`
#define __NR_rt_sigqueueinfo        178 ///< System-call number for `rt_sigqueueinfo`
#define __NR_rt_sigsuspend          179 ///< System-call number for `rt_sigsuspend`
#define __NR_pread                  180 ///< System-call number for `pread`
#define __NR_pwrite                 181 ///< System-call number for `pwrite`
#define __NR_chown                  182 ///< System-call number for `chown`
#define __NR_getcwd                 183 ///< System-call number for `getcwd`
#define __NR_capget                 184 ///< System-call number for `capget`
#define __NR_capset                 185 ///< System-call number for `capset`
#define __NR_sigaltstack            186 ///< System-call number for `sigaltstack`
#define __NR_sendfile               187 ///< System-call number for `sendfile`
#define __NR_waitperiod             188 ///< System-call number for `waitperiod`
#define __NR_msgctl                 189 ///<  System-call number for `msgctl`
#define __NR_msgget                 190 ///<  System-call number for `msgget`
#define __NR_msgrcv                 191 ///<  System-call number for `msgrcv`
#define __NR_msgsnd                 192 ///<  System-call number for `msgsnd`
#define __NR_semctl                 193 ///<  System-call number for `semctl`
#define __NR_semget                 194 ///<  System-call number for `semget`
#define __NR_semop                  195 ///<  System-call number for `semop`
#define __NR_shmat                  196 ///<  System-call number for `shmat`
#define __NR_shmctl                 197 ///<  System-call number for `shmctl`
#define __NR_shmdt                  198 ///<  System-call number for `shmdt`
#define __NR_shmget                 199 ///<  System-call number for `shmget`
#define SYSCALL_NUMBER              200 ///< The total number of system-calls.

/// @brief Handle the value returned from a system call.
/// @param type Specifies the type of the returned value.
/// @param res  The name of the variable where the result of the SC is stored.
#define __syscall_return(type, res)                        \
    do {                                                   \
        if ((unsigned int)(res) >= (unsigned int)(-125)) { \
            errno = -(res);                                \
            (res) = -1;                                    \
        }                                                  \
        return (type)(res);                                \
    } while (0)

/// @brief Heart of the code that calls a system call with 0 parameters.
#define __inline_syscall0(res, name) \
    __asm__ __volatile__("int $0x80" \
                         : "=a"(res) \
                         : "0"(__NR_##name))

/// @brief Heart of the code that calls a system call with 1 parameter.
#define __inline_syscall1(res, name, arg1)                                    \
    __asm__ __volatile__("push %%ebx ; movl %2,%%ebx ; int $0x80 ; pop %%ebx" \
                         : "=a"(res)                                          \
                         : "0"(__NR_##name), "ri"((int)(arg1))                \
                         : "memory");

/// @brief Heart of the code that calls a system call with 2 parameters.
#define __inline_syscall2(res, name, arg1, arg2)                                 \
    __asm__ __volatile__("push %%ebx ; movl %2,%%ebx ; int $0x80 ; pop %%ebx"    \
                         : "=a"(res)                                             \
                         : "0"(__NR_##name), "ri"((int)(arg1)), "c"((int)(arg2)) \
                         : "memory");

/// @brief Heart of the code that calls a system call with 3 parameters.
#define __inline_syscall3(res, name, arg1, arg2, arg3)                            \
    __asm__ __volatile__("push %%ebx ; movl %2,%%ebx ; int $0x80 ; pop %%ebx"     \
                         : "=a"(res)                                              \
                         : "0"(__NR_##name), "ri"((int)(arg1)), "c"((int)(arg2)), \
                           "d"((int)(arg3))                                       \
                         : "memory");

/// @brief Heart of the code that calls a system call with 4 parameters.
#define __inline_syscall4(res, name, arg1, arg2, arg3, arg4)                      \
    __asm__ __volatile__("push %%ebx ; movl %2,%%ebx ; int $0x80 ; pop %%ebx"     \
                         : "=a"(res)                                              \
                         : "0"(__NR_##name), "ri"((int)(arg1)), "c"((int)(arg2)), \
                           "d"((int)(arg3)), "S"((int)(arg4))                     \
                         : "memory");

/// @brief Heart of the code that calls a system call with 5 parameters.
#define __inline_syscall5(res, name, arg1, arg2, arg3, arg4, arg5)                \
    __asm__ __volatile__("push %%ebx ; movl %2,%%ebx ; movl %1,%%eax ; "          \
                         "int $0x80 ; pop %%ebx"                                  \
                         : "=a"(res)                                              \
                         : "i"(__NR_##name), "ri"((int)(arg1)), "c"((int)(arg2)), \
                           "d"((int)(arg3)), "S"((int)(arg4)), "D"((int)(arg5))   \
                         : "memory");

/// @brief System call with 0 parameters.
#define _syscall0(type, name)           \
    type name(void)                     \
    {                                   \
        long __res;                     \
        __inline_syscall0(__res, name); \
        __syscall_return(type, __res);  \
    }

/// @brief System call with 1 parameter.
#define _syscall1(type, name, type1, arg1)    \
    type name(type1 arg1)                     \
    {                                         \
        long __res;                           \
        __inline_syscall1(__res, name, arg1); \
        __syscall_return(type, __res);        \
    }

/// @brief System call with 2 parameters.
#define _syscall2(type, name, type1, arg1, type2, arg2) \
    type name(type1 arg1, type2 arg2)                   \
    {                                                   \
        long __res;                                     \
        __inline_syscall2(__res, name, arg1, arg2);     \
        __syscall_return(type, __res);                  \
    }

/// @brief System call with 3 parameters.
#define _syscall3(type, name, type1, arg1, type2, arg2, type3, arg3) \
    type name(type1 arg1, type2 arg2, type3 arg3)                    \
    {                                                                \
        long __res;                                                  \
        __inline_syscall3(__res, name, arg1, arg2, arg3);            \
        __syscall_return(type, __res);                               \
    }

/// @brief System call with 4 parameters.
#define _syscall4(type, name, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
    type name(type1 arg1, type2 arg2, type3 arg3, type4 arg4)                     \
    {                                                                             \
        long __res;                                                               \
        __inline_syscall4(__res, name, arg1, arg2, arg3, arg4);                   \
        __syscall_return(type, __res);                                            \
    }

/// @brief System call with 5 parameters.
#define _syscall5(type, name, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5) \
    type name(type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5)                      \
    {                                                                                          \
        long __res;                                                                            \
        __inline_syscall5(__res, name, arg1, arg2, arg3, arg4, arg5);                          \
        __syscall_return(type, __res);                                                         \
    }
