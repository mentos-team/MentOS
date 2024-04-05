# System Call

## Legenda

The **first column** of the table represent the implementation status,
specifically:

1. Implemented
2. Tested
X. Obsolete

If the column is empty it means that it's not implemented yet.

## Implementation Status

```
 S | EAX | Name                       | Source                      | EBX                      | ECX                          | EDX                     | ESX             | EDI              |
---+-----+----------------------------+-----------------------------+--------------------------+------------------------------+-------------------------+-----------------+------------------|
 2 | 1   | sys_exit                   | process/scheduler.c         | int                      | -                            | -                       | -               | -                |
 2 | 2   | sys_fork                   | process/process.c           | struct pt_regs           | -                            | -                       | -               | -                |
 2 | 3   | sys_read                   | fs/read_write.c             | unsigned int             | char *                       | size_t                  | -               | -                |
 2 | 4   | sys_write                  | fs/read_write.c             | unsigned int             | const char *                 | size_t                  | -               | -                |
 2 | 5   | sys_open                   | fs/open.c                   | const char *             | int                          | int                     | -               | -                |
 2 | 6   | sys_close                  | fs/open.c                   | int                      | -                            | -                       | -               | -                |
 2 | 7   | sys_waitpid                | process/scheduler.c         | pid_t                    | unsigned int *               | int                     | -               | -                |
 2 | 8   | sys_creat                  | fs/namei.c                  | const char *             | int                          | -                       | -               | -                |
   | 9   | sys_link                   |                             | const char *             | const char *                 | -                       | -               | -                |
 2 | 10  | sys_unlink                 | fs/namei.c                  | const char *             | -                            | -                       | -               | -                |
 2 | 11  | sys_execve                 | process/process.c           | struct pt_regs           | -                            | -                       | -               | -                |
 2 | 12  | sys_chdir                  | process/process.c           | const char *             | -                            | -                       | -               | -                |
 2 | 13  | sys_time                   | klib/time.c                 | int *                    | -                            | -                       | -               | -                |
   | 14  | sys_mknod                  |                             | const char *             | int                          | dev_t                   | -               | -                |
 1 | 15  | sys_chmod                  | fs/attr.c                   | const char *             | mode_t                       | -                       | -               | -                |
   | 16  | sys_lchown                 | fs/attr.c                   | const char *             | uid_t                        | gid_t                   | -               | -                |
 2 | 18  | sys_stat                   | fs/stat.c                   | char *                   | struct __old_kernel_stat *   | -                       | -               | -                |
 2 | 19  | sys_lseek                  | fs/read_write.c             | unsigned int             | off_t                        | unsigned int            | -               | -                |
 2 | 20  | sys_getpid                 | process/scheduler.c         | -                        | -                            | -                       | -               | -                |
   | 21  | sys_mount                  |                             | char *                   | char *                       | char *                  | -               | -                |
   | 22  | sys_oldumount              |                             | char *                   | -                            | -                       | -               | -                |
 2 | 23  | sys_setuid                 | process/scheduler.c         | uid_t                    | -                            | -                       | -               | -                |
 2 | 24  | sys_getuid                 | process/scheduler.c         | -                        | -                            | -                       | -               | -                |
   | 25  | sys_stime                  |                             | int *                    | -                            | -                       | -               | -                |
   | 26  | sys_ptrace                 |                             | long                     | long                         | long                    | long            | -                |
 2 | 27  | sys_alarm                  | hardware/timer.c            | unsigned int             | -                            | -                       | -               | -                |
 2 | 28  | sys_fstat                  | fs/stat.c                   | unsigned int             | struct __old_kernel_stat *   | -                       | -               | -                |
   | 29  | sys_pause                  |                             | -                        | -                            | -                       | -               | -                |
   | 30  | sys_utime                  |                             | char *                   | struct utimbuf *             | -                       | -               | -                |
   | 33  | sys_access                 |                             | const char *             | int                          | -                       | -               | -                |
 1 | 34  | sys_nice                   | process/scheduler.c         | int                      | -                            | -                       | -               | -                |
   | 36  | sys_sync                   |                             | -                        | -                            | -                       | -               | -                |
 1 | 37  | sys_kill                   | system/signal.c             | int                      | int                          | -                       | -               | -                |
   | 38  | sys_rename                 |                             | const char *             | const char *                 | -                       | -               | -                |
 2 | 39  | sys_mkdir                  | fs/namei.c                  | const char *             | int                          | -                       | -               | -                |
 2 | 40  | sys_rmdir                  | fs/namei.c                  | const char *             | -                            | -                       | -               | -                |
 1 | 41  | sys_dup                    | fs/vfs.c                    | unsigned int             | -                            | -                       | -               | -                |
   | 42  | sys_pipe                   |                             | unsigned long *          | -                            | -                       | -               | -                |
   | 43  | sys_times                  |                             | struct tms *             | -                            | -                       | -               | -                |
 2 | 45  | sys_brk                    | mem/kheap.c                 | unsigned long            | -                            | -                       | -               | -                |
 2 | 46  | sys_setgid                 | process/scheduler.c         | gid_t                    | -                            | -                       | -               | -                |
 2 | 47  | sys_getgid                 | process/scheduler.c         | -                        | -                            | -                       | -               | -                |
 1 | 48  | sys_signal                 | system/signal.c             | int                      | __sighandler_t               | -                       | -               | -                |
 1 | 49  | sys_geteuid                | process/scheduler.c         | -                        | -                            | -                       | -               | -                |
 1 | 50  | sys_getegid                | process/scheduler.c         | -                        | -                            | -                       | -               | -                |
   | 51  | sys_acct                   |                             | const char *             | -                            | -                       | -               | -                |
   | 52  | sys_umount                 |                             | char *                   | int                          | -                       | -               | -                |
 1 | 54  | sys_ioctl                  | fs/ioctl.c                  | unsigned int             | unsigned int                 | unsigned long           | -               | -                |
   | 55  | sys_fcntl                  |                             | unsigned int             | unsigned int                 | unsigned long           | -               | -                |
 2 | 57  | sys_setpgid                | process/scheduler.c         | pid_t                    | pid_t                        | -                       | -               | -                |
   | 59  | sys_olduname               |                             | struct oldold_utsname *  | -                            | -                       | -               | -                |
   | 60  | sys_umask                  |                             | int                      | -                            | -                       | -               | -                |
   | 61  | sys_chroot                 |                             | const char *             | -                            | -                       | -               | -                |
   | 62  | sys_ustat                  |                             | dev_t                    | struct ustat *               | -                       | -               | -                |
   | 63  | sys_dup2                   |                             | unsigned int             | unsigned int                 | -                       | -               | -                |
 2 | 64  | sys_getppid                | process/scheduler.c         | -                        | -                            | -                       | -               | -                |
   | 65  | sys_getpgrp                |                             | -                        | -                            | -                       | -               | -                |
 2 | 66  | sys_setsid                 | process/scheduler.c         | -                        | -                            | -                       | -               | -                |
 1 | 67  | sys_sigaction              | system/signal.c             | int                      | const struct old_sigaction * | struct old_sigaction *  | -               | -                |
 X | 68  | sys_sgetmask               |                             | -                        | -                            | -                       | -               | -                |
 X | 69  | sys_ssetmask               |                             | int                      | -                            | -                       | -               | -                |
 1 | 70  | sys_setreuid               | process/scheduler.c         | uid_t                    | uid_t                        | -                       | -               | -                |
 1 | 71  | sys_setregid               | process/scheduler.c         | gid_t                    | gid_t                        | -                       | -               | -                |
   | 72  | sys_sigsuspend             |                             | int                      | int                          | old_sigset_t            | -               | -                |
   | 73  | sys_sigpending             |                             | old_sigset_t *           | -                            | -                       | -               | -                |
   | 74  | sys_sethostname            |                             | char *                   | int                          | -                       | -               | -                |
   | 75  | sys_setrlimit              |                             | unsigned int             | struct rlimit *              | -                       | -               | -                |
   | 76  | sys_getrlimit              |                             | unsigned int             | struct rlimit *              | -                       | -               | -                |
   | 77  | sys_getrusage              |                             | int                      | struct rusage *              | -                       | -               | -                |
   | 78  | sys_gettimeofday           |                             | struct timeval *         | struct timezone *            | -                       | -               | -                |
   | 79  | sys_settimeofday           |                             | struct timeval *         | struct timezone *            | -                       | -               | -                |
   | 80  | sys_getgroups              |                             | int                      | gid_t *                      | -                       | -               | -                |
   | 81  | sys_setgroups              |                             | int                      | gid_t *                      | -                       | -               | -                |
   | 83  | sys_symlink                |                             | const char *             | const char *                 | -                       | -               | -                |
   | 84  | sys_lstat                  |                             | char *                   | struct __old_kernel_stat *   | -                       | -               | -                |
   | 85  | sys_readlink               |                             | const char *             | char *                       | int                     | -               | -                |
   | 86  | sys_uselib                 |                             | const char *             | -                            | -                       | -               | -                |
   | 87  | sys_swapon                 |                             | const char *             | int                          | -                       | -               | -                |
 1 | 88  | sys_reboot                 | kernel/sys.c                | int                      | int                          | int                     | void *          | -                |
   | 89  | old_readdir                |                             | unsigned int             | void *                       | unsigned int            | -               | -                |
   | 90  | old_mmap                   |                             | struct mmap_arg_struct * | -                            | -                       | -               | -                |
   | 91  | sys_munmap                 |                             | unsigned long            | size_t                       | -                       | -               | -                |
   | 92  | sys_truncate               |                             | const char *             | unsigned long                | -                       | -               | -                |
   | 93  | sys_ftruncate              |                             | unsigned int             | unsigned long                | -                       | -               | -                |
   | 94  | sys_fchmod                 |                             | unsigned int             | mode_t                       | -                       | -               | -                |
 1 | 95  | sys_fchown                 | fs/attr.c                   | unsigned int             | uid_t                        | gid_t                   | -               | -                |
   | 96  | sys_getpriority            |                             | int                      | int                          | -                       | -               | -                |
   | 97  | sys_setpriority            |                             | int                      | int                          | int                     | -               | -                |
   | 99  | sys_statfs                 |                             | const char *             | struct statfs *              | -                       | -               | -                |
   | 100 | sys_fstatfs                |                             | unsigned int             | struct statfs *              | -                       | -               | -                |
   | 101 | sys_ioperm                 |                             | unsigned long            | unsigned long                | int                     | -               | -                |
   | 102 | sys_socketcall             |                             | int                      | unsigned long *              | -                       | -               | -                |
 1 | 103 | sys_syslog                 | system/printk.c             | int                      | char *                       | int                     | -               | -                |
 1 | 104 | sys_setitimer              | hardware/timer.c            | int                      | struct itimerval *           | struct itimerval *      | -               | -                |
 1 | 105 | sys_getitimer              | hardware/timer.c            | int                      | struct itimerval *           | -                       | -               | -                |
   | 106 | sys_newstat                |                             | char *                   | struct stat *                | -                       | -               | -                |
   | 107 | sys_newlstat               |                             | char *                   | struct stat *                | -                       | -               | -                |
   | 108 | sys_newfstat               |                             | unsigned int             | struct stat *                | -                       | -               | -                |
 1 | 109 | sys_uname                  | sys/utsname.c               | struct old_utsname *     | -                            | -                       | -               | -                |
   | 110 | sys_iopl                   |                             | unsigned long            | -                            | -                       | -               | -                |
   | 111 | sys_vhangup                |                             | -                        | -                            | -                       | -               | -                |
   | 112 | sys_idle                   |                             | -                        | -                            | -                       | -               | -                |
   | 113 | sys_vm86old                |                             | unsigned long            | struct vm86plus_struct *     | -                       | -               | -                |
   | 114 | sys_wait4                  |                             | pid_t                    | unsigned long *              | int options             | struct rusage * | -                |
   | 115 | sys_swapoff                |                             | const char *             | -                            | -                       | -               | -                |
   | 116 | sys_sysinfo                |                             | struct sysinfo *         | -                            | -                       | -               | -                |
 * | 117 | sys_ipc(*Note)             |                             | uint                     | int                          | int                     | int             | void *           |
   | 118 | sys_fsync                  |                             | unsigned int             | -                            | -                       | -               | -                |
 1 | 119 | sys_sigreturn              | system/signal.c             | unsigned long            | -                            | -                       | -               | -                |
   | 120 | sys_clone                  |                             | struct pt_regs           | -                            | -                       | -               | -                |
   | 121 | sys_setdomainname          |                             | char *                   | int                          | -                       | -               | -                |
   | 122 | sys_newuname               |                             | struct new_utsname *     | -                            | -                       | -               | -                |
   | 123 | sys_modify_ldt             |                             | int                      | void *                       | unsigned long           | -               | -                |
   | 124 | sys_adjtimex               |                             | struct timex *           | -                            | -                       | -               | -                |
   | 125 | sys_mprotect               |                             | unsigned long            | size_t                       | unsigned long           | -               | -                |
 1 | 126 | sys_sigprocmask            | system/signal.c             | int                      | old_sigset_t *               | old_sigset_t *          | -               | -                |
   | 127 | sys_create_module          |                             | const char *             | size_t                       | -                       | -               | -                |
   | 128 | sys_init_module            |                             | const char *             | struct module *              | -                       | -               | -                |
   | 129 | sys_delete_module          |                             | const char *             | -                            | -                       | -               | -                |
   | 130 | sys_get_kernel_syms        |                             | struct kernel_sym *      | -                            | -                       | -               | -                |
   | 131 | sys_quotactl               |                             | int                      | const char *                 | int                     | caddr_t         | -                |
   | 132 | sys_getpgid                |                             | pid_t                    | -                            | -                       | -               | -                |
 1 | 133 | sys_fchdir                 | process/process.c           | unsigned int             | -                            | -                       | -               | -                |
   | 134 | sys_bdflush                |                             | int                      | long                         | -                       | -               | -                |
   | 135 | sys_sysfs                  |                             | int                      | unsigned long                | unsigned long           | -               | -                |
   | 136 | sys_personality            |                             | unsigned long            | -                            | -                       | -               | -                |
   | 138 | sys_setfsuid               |                             | uid_t                    | -                            | -                       | -               | -                |
   | 139 | sys_setfsgid               |                             | gid_t                    | -                            | -                       | -               | -                |
   | 140 | sys_llseek                 |                             | unsigned int             | unsigned long                | unsigned long           | loff_t *        | unsigned int     |
 2 | 141 | sys_getdents               | fs/readdir.c                | unsigned int             | void *                       | unsigned int            | -               | -                |
   | 142 | sys_select                 |                             | int                      | fd_set *                     | fd_set *                | fd_set *        | struct timeval * |
   | 143 | sys_flock                  |                             | unsigned int             | unsigned int                 | -                       | -               | -                |
   | 144 | sys_msync                  |                             | unsigned long            | size_t                       | int                     | -               | -                |
   | 145 | sys_readv                  |                             | unsigned long            | const struct iovec *         | unsigned long           | -               | -                |
   | 146 | sys_writev                 |                             | unsigned long            | const struct iovec *         | unsigned long           | -               | -                |
 1 | 147 | sys_getsid                 | process/scheduler.c         | pid_t                    | -                            | -                       | -               | -                |
   | 148 | sys_fdatasync              |                             | unsigned int             | -                            | -                       | -               | -                |
   | 149 | sys_sysctl                 |                             | struct __sysctl_args *   | -                            | -                       | -               | -                |
   | 150 | sys_mlock                  |                             | unsigned long            | size_t                       | -                       | -               | -                |
   | 151 | sys_munlock                |                             | unsigned long            | size_t                       | -                       | -               | -                |
   | 152 | sys_mlockall               |                             | int                      | -                            | -                       | -               | -                |
   | 153 | sys_munlockall             |                             | -                        | -                            | -                       | -               | -                |
 1 | 154 | sys_sched_setparam         | process/scheduler.c         | pid_t                    | struct sched_param *         | -                       | -               | -                |
 1 | 155 | sys_sched_getparam         | process/scheduler.c         | pid_t                    | struct sched_param *         | -                       | -               | -                |
   | 156 | sys_sched_setscheduler     |                             | pid_t                    | int                          | struct sched_param *    | -               | -                |
   | 157 | sys_sched_getscheduler     |                             | pid_t                    | -                            | -                       | -               | -                |
   | 158 | sys_sched_yield            |                             | -                        | -                            | -                       | -               | -                |
   | 159 | sys_sched_get_priority_max |                             | int                      | -                            | -                       | -               | -                |
   | 160 | sys_sched_get_priority_min |                             | int                      | -                            | -                       | -               | -                |
   | 161 | sys_sched_rr_get_interval  |                             | pid_t                    | struct timespec *            | -                       | -               | -                |
 1 | 162 | sys_nanosleep              | hardware/timer.c            | struct timespec *        | struct timespec *            | -                       | -               | -                |
   | 163 | sys_mremap                 |                             | unsigned long            | unsigned long                | unsigned long           | unsigned long   | -                |
   | 164 | sys_setresuid              |                             | uid_t                    | uid_t                        | uid_t                   | -               | -                |
   | 165 | sys_getresuid              |                             | uid_t *                  | uid_t *                      | uid_t *                 | -               | -                |
   | 166 | sys_vm86                   |                             | struct vm86_struct *     | -                            | -                       | -               | -                |
   | 167 | sys_query_module           |                             | const char *             | int                          | char *                  | size_t          | size_t *         |
   | 168 | sys_poll                   |                             | struct pollfd *          | unsigned int                 | long                    | -               | -                |
   | 169 | sys_nfsservctl             |                             | int                      | void *                       | void *                  | -               | -                |
   | 170 | sys_setresgid              |                             | gid_t                    | gid_t                        | gid_t                   | -               | -                |
   | 171 | sys_getresgid              |                             | gid_t *                  | gid_t *                      | gid_t *                 | -               | -                |
   | 172 | sys_prctl                  |                             | int                      | unsigned long                | unsigned long           | unsigned long   | unsigned long    |
   | 173 | sys_rt_sigreturn           |                             | unsigned long            | -                            | -                       | -               | -                |
   | 174 | sys_rt_sigaction           |                             | int                      | const struct sigaction *     | struct sigaction *      | size_t          | -                |
   | 175 | sys_rt_sigprocmask         |                             | int                      | sigset_t *                   | sigset_t *              | size_t          | -                |
   | 176 | sys_rt_sigpending          |                             | sigset_t *               | size_t                       | -                       | -               | -                |
   | 177 | sys_rt_sigtimedwait        |                             | const sigset_t *         | siginfo_t *                  | const struct timespec * | size_t          | -                |
   | 178 | sys_rt_sigqueueinfo        |                             | int                      | int                          | siginfo_t *             | -               | -                |
   | 179 | sys_rt_sigsuspend          |                             | sigset_t *               | size_t                       | -                       | -               | -                |
   | 180 | sys_pread                  |                             | unsigned int             | char *                       | size_t                  | loff_t          | -                |
   | 181 | sys_pwrite                 |                             | unsigned int             | const char *                 | size_t                  | loff_t          | -                |
 1 | 182 | sys_chown                  | vfs/attr.c                   | const char *             | uid_t                        | gid_t                   | -               | -                |
 1 | 183 | sys_getcwd                 | process/process.c           | char *                   | unsigned long                | -                       | -               | -                |
   | 184 | sys_capget                 |                             | cap_user_header_t        | cap_user_data_t              | -                       | -               | -                |
   | 185 | sys_capset                 |                             | cap_user_header_t        | const cap_user_data_t        | -                       | -               | -                |
   | 186 | sys_sigaltstack            |                             | const stack_t *          | stack_t *                    | -                       | -               | -                |
   | 187 | sys_sendfile               |                             | int                      | int                          | off_t *                 | size_t          | -                |
 2 | 188 | sys_waitperiod             | process/scheduler.c         |                          |                              |                         |                 |                  |
   | 189 | sys_msgctl                 |                             | int                      |                              |                         |                 |                  |
   | 190 | sys_msgget                 |                             |                          |                              |                         |                 |                  |
   | 191 | sys_msgrcv                 |                             |                          |                              |                         |                 |                  |
   | 192 | sys_msgsnd                 |                             |                          |                              |                         |                 |                  |
   | 193 | sys_semctl                 |                             |                          |                              |                         |                 |                  |
   | 194 | sys_semget                 |                             |                          |                              |                         |                 |                  |
   | 195 | sys_semop                  |                             |                          |                              |                         |                 |                  |
   | 196 | sys_shmat                  |                             |                          |                              |                         |                 |                  |
   | 197 | sys_shmctl                 |                             |                          |                              |                         |                 |                  |
   | 198 | sys_shmdt                  |                             |                          |                              |                         |                 |                  |
   | 199 | sys_shmget                 |                             |                          |                              |                         |                 |                  |
```