/// @file syscall_types.h
/// @brief System Call numbers.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
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
#define __NR_setuid16               23  ///< System-call number for `setuid16`
#define __NR_getuid16               24  ///< System-call number for `getuid16`
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
#define __NR_setgid16               46  ///< System-call number for `setgid16`
#define __NR_getgid16               47  ///< System-call number for `getgid16`
#define __NR_signal                 48  ///< System-call number for `signal`
#define __NR_geteuid16              49  ///< System-call number for `geteuid16`
#define __NR_getegid16              50  ///< System-call number for `getegid16`
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
#define __NR_setreuid16             70  ///< System-call number for `setreuid16`
#define __NR_setregid16             71  ///< System-call number for `setregid16`
#define __NR_sigsuspend             72  ///< System-call number for `sigsuspend`
#define __NR_sigpending             73  ///< System-call number for `sigpending`
#define __NR_sethostname            74  ///< System-call number for `sethostname`
#define __NR_setrlimit              75  ///< System-call number for `setrlimit`
#define __NR_getrlimit              76  ///< System-call number for `getrlimit`
#define __NR_getrusage              77  ///< System-call number for `getrusage`
#define __NR_gettimeofday           78  ///< System-call number for `gettimeofday`
#define __NR_settimeofday           79  ///< System-call number for `settimeofday`
#define __NR_getgroups16            80  ///< System-call number for `getgroups16`
#define __NR_setgroups16            81  ///< System-call number for `setgroups16`
#define __NR_symlink                83  ///< System-call number for `symlink`
#define __NR_lstat                  84  ///< System-call number for `lstat`
#define __NR_readlink               85  ///< System-call number for `readlink`
#define __NR_uselib                 86  ///< System-call number for `uselib`
#define __NR_swapon                 87  ///< System-call number for `swapon`
#define __NR_reboot                 88  ///< System-call number for `reboot`
#define __NR_readdir                89  ///< System-call number for `old_readdir`
#define __NR_mmap                   90  ///< System-call number for `old_mmap`
#define __NR_munmap                 91  ///< System-call number for `munmap`
#define __NR_truncate               92  ///< System-call number for `truncate`
#define __NR_ftruncate              93  ///< System-call number for `ftruncate`
#define __NR_fchmod                 94  ///< System-call number for `fchmod`
#define __NR_fchown16               95  ///< System-call number for `fchown16`
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
#define __NR_init_module            128 ///< System-call number for `init_module`
#define __NR_delete_module          129 ///< System-call number for `delete_module`
#define __NR_quotactl               131 ///< System-call number for `quotactl`
#define __NR_getpgid                132 ///< System-call number for `getpgid`
#define __NR_fchdir                 133 ///< System-call number for `fchdir`
#define __NR_bdflush                134 ///< System-call number for `bdflush`
#define __NR_sysfs                  135 ///< System-call number for `sysfs`
#define __NR_personality            136 ///< System-call number for `personality`
#define __NR_setfsuid16             138 ///< System-call number for `setfsuid16`
#define __NR_setfsgid16             139 ///< System-call number for `setfsgid16`
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
#define __NR_setresuid16            164 ///< System-call number for `setresuid16`
#define __NR_getresuid16            165 ///< System-call number for `getresuid16`
#define __NR_vm86                   166 ///< System-call number for `vm86`
#define __NR_poll                   168 ///< System-call number for `poll`
#define __NR_setresgid16            170 ///< System-call number for `setresgid16`
#define __NR_getresgid16            171 ///< System-call number for `getresgid16`
#define __NR_prctl                  172 ///< System-call number for `prctl`
#define __NR_rt_sigreturn           173 ///< System-call number for `rt_sigreturn`
#define __NR_rt_sigaction           174 ///< System-call number for `rt_sigaction`
#define __NR_rt_sigprocmask         175 ///< System-call number for `rt_sigprocmask`
#define __NR_rt_sigpending          176 ///< System-call number for `rt_sigpending`
#define __NR_rt_sigtimedwait        177 ///< System-call number for `rt_sigtimedwait`
#define __NR_rt_sigqueueinfo        178 ///< System-call number for `rt_sigqueueinfo`
#define __NR_rt_sigsuspend          179 ///< System-call number for `rt_sigsuspend`
#define __NR_pread64                180 ///< System-call number for `pread64`
#define __NR_pwrite64               181 ///< System-call number for `pwrite64`
#define __NR_chown16                182 ///< System-call number for `chown16`
#define __NR_getcwd                 183 ///< System-call number for `getcwd`
#define __NR_capget                 184 ///< System-call number for `capget`
#define __NR_capset                 185 ///< System-call number for `capset`
#define __NR_sigaltstack            186 ///< System-call number for `sigaltstack`
#define __NR_sendfile               187 ///< System-call number for `sendfile`
#define __NR_vfork                  190 ///< System-call number for `vfork`
#define __NR_mmap_pgoff             192 ///< System-call number for `mmap_pgoff`
#define __NR_truncate64             193 ///< System-call number for `truncate64`
#define __NR_ftruncate64            194 ///< System-call number for `ftruncate64`
#define __NR_stat64                 195 ///< System-call number for `stat64`
#define __NR_lstat64                196 ///< System-call number for `lstat64`
#define __NR_fstat64                197 ///< System-call number for `fstat64`
#define __NR_getuid                 199 ///< System-call number for `getuid`
#define __NR_getgid                 200 ///< System-call number for `getgid`
#define __NR_geteuid                201 ///< System-call number for `geteuid`
#define __NR_getegid                202 ///< System-call number for `getegid`
#define __NR_setreuid               203 ///< System-call number for `setreuid`
#define __NR_setregid               204 ///< System-call number for `setregid`
#define __NR_getgroups              205 ///< System-call number for `getgroups`
#define __NR_setgroups              206 ///< System-call number for `setgroups`
#define __NR_fchown                 207 ///< System-call number for `fchown`
#define __NR_setresuid              208 ///< System-call number for `setresuid`
#define __NR_getresuid              209 ///< System-call number for `getresuid`
#define __NR_setresgid              210 ///< System-call number for `setresgid`
#define __NR_getresgid              211 ///< System-call number for `getresgid`
#define __NR_chown                  212 ///< System-call number for `chown`
#define __NR_setuid                 213 ///< System-call number for `setuid`
#define __NR_setgid                 214 ///< System-call number for `setgid`
#define __NR_setfsuid               215 ///< System-call number for `setfsuid`
#define __NR_setfsgid               216 ///< System-call number for `setfsgid`
#define __NR_pivot_root             217 ///< System-call number for `pivot_root`
#define __NR_mincore                218 ///< System-call number for `mincore`
#define __NR_madvise                219 ///< System-call number for `madvise`
#define __NR_getdents64             220 ///< System-call number for `getdents64`
#define __NR_fcntl64                221 ///< System-call number for `fcntl64`
#define __NR_gettid                 224 ///< System-call number for `gettid`
#define __NR_readahead              225 ///< System-call number for `readahead`
#define __NR_setxattr               226 ///< System-call number for `setxattr`
#define __NR_lsetxattr              227 ///< System-call number for `lsetxattr`
#define __NR_fsetxattr              228 ///< System-call number for `fsetxattr`
#define __NR_getxattr               229 ///< System-call number for `getxattr`
#define __NR_lgetxattr              230 ///< System-call number for `lgetxattr`
#define __NR_fgetxattr              231 ///< System-call number for `fgetxattr`
#define __NR_listxattr              232 ///< System-call number for `listxattr`
#define __NR_llistxattr             233 ///< System-call number for `llistxattr`
#define __NR_flistxattr             234 ///< System-call number for `flistxattr`
#define __NR_removexattr            235 ///< System-call number for `removexattr`
#define __NR_lremovexattr           236 ///< System-call number for `lremovexattr`
#define __NR_fremovexattr           237 ///< System-call number for `fremovexattr`
#define __NR_tkill                  238 ///< System-call number for `tkill`
#define __NR_sendfile64             239 ///< System-call number for `sendfile64`
#define __NR_futex                  240 ///< System-call number for `futex`
#define __NR_sched_setaffinity      241 ///< System-call number for `sched_setaffinity`
#define __NR_sched_getaffinity      242 ///< System-call number for `sched_getaffinity`
#define __NR_set_thread_area        243 ///< System-call number for `set_thread_area`
#define __NR_get_thread_area        244 ///< System-call number for `get_thread_area`
#define __NR_io_setup               245 ///< System-call number for `io_setup`
#define __NR_io_destroy             246 ///< System-call number for `io_destroy`
#define __NR_io_getevents           247 ///< System-call number for `io_getevents`
#define __NR_io_submit              248 ///< System-call number for `io_submit`
#define __NR_io_cancel              249 ///< System-call number for `io_cancel`
#define __NR_fadvise64              250 ///< System-call number for `fadvise64`
#define __NR_exit_group             252 ///< System-call number for `exit_group`
#define __NR_lookup_dcookie         253 ///< System-call number for `lookup_dcookie`
#define __NR_epoll_create           254 ///< System-call number for `epoll_create`
#define __NR_epoll_ctl              255 ///< System-call number for `epoll_ctl`
#define __NR_epoll_wait             256 ///< System-call number for `epoll_wait`
#define __NR_remap_file_pages       257 ///< System-call number for `remap_file_pages`
#define __NR_set_tid_address        258 ///< System-call number for `set_tid_address`
#define __NR_timer_create           259 ///< System-call number for `timer_create`
#define __NR_timer_settime          260 ///< System-call number for `timer_settime`
#define __NR_timer_gettime          261 ///< System-call number for `timer_gettime`
#define __NR_timer_getoverrun       262 ///< System-call number for `timer_getoverrun`
#define __NR_timer_delete           263 ///< System-call number for `timer_delete`
#define __NR_clock_settime          264 ///< System-call number for `clock_settime`
#define __NR_clock_gettime          265 ///< System-call number for `clock_gettime`
#define __NR_clock_getres           266 ///< System-call number for `clock_getres`
#define __NR_clock_nanosleep        267 ///< System-call number for `clock_nanosleep`
#define __NR_statfs64               268 ///< System-call number for `statfs64`
#define __NR_fstatfs64              269 ///< System-call number for `fstatfs64`
#define __NR_tgkill                 270 ///< System-call number for `tgkill`
#define __NR_utimes                 271 ///< System-call number for `utimes`
#define __NR_fadvise64_64           272 ///< System-call number for `fadvise64_64`
#define __NR_mbind                  274 ///< System-call number for `mbind`
#define __NR_get_mempolicy          275 ///< System-call number for `get_mempolicy`
#define __NR_set_mempolicy          276 ///< System-call number for `set_mempolicy`
#define __NR_mq_open                277 ///< System-call number for `mq_open`
#define __NR_mq_unlink              278 ///< System-call number for `mq_unlink`
#define __NR_mq_timedsend           279 ///< System-call number for `mq_timedsend`
#define __NR_mq_timedreceive        280 ///< System-call number for `mq_timedreceive`
#define __NR_mq_notify              281 ///< System-call number for `mq_notify`
#define __NR_mq_getsetattr          282 ///< System-call number for `mq_getsetattr`
#define __NR_kexec_load             283 ///< System-call number for `kexec_load`
#define __NR_waitid                 284 ///< System-call number for `waitid`
#define __NR_add_key                286 ///< System-call number for `add_key`
#define __NR_request_key            287 ///< System-call number for `request_key`
#define __NR_keyctl                 288 ///< System-call number for `keyctl`
#define __NR_ioprio_set             289 ///< System-call number for `ioprio_set`
#define __NR_ioprio_get             290 ///< System-call number for `ioprio_get`
#define __NR_inotify_init           291 ///< System-call number for `inotify_init`
#define __NR_inotify_add_watch      292 ///< System-call number for `inotify_add_watch`
#define __NR_inotify_rm_watch       293 ///< System-call number for `inotify_rm_watch`
#define __NR_migrate_pages          294 ///< System-call number for `migrate_pages`
#define __NR_openat                 295 ///< System-call number for `openat`
#define __NR_mkdirat                296 ///< System-call number for `mkdirat`
#define __NR_mknodat                297 ///< System-call number for `mknodat`
#define __NR_fchownat               298 ///< System-call number for `fchownat`
#define __NR_futimesat              299 ///< System-call number for `futimesat`
#define __NR_fstatat64              300 ///< System-call number for `fstatat64`
#define __NR_unlinkat               301 ///< System-call number for `unlinkat`
#define __NR_renameat               302 ///< System-call number for `renameat`
#define __NR_linkat                 303 ///< System-call number for `linkat`
#define __NR_symlinkat              304 ///< System-call number for `symlinkat`
#define __NR_readlinkat             305 ///< System-call number for `readlinkat`
#define __NR_fchmodat               306 ///< System-call number for `fchmodat`
#define __NR_faccessat              307 ///< System-call number for `faccessat`
#define __NR_pselect6               308 ///< System-call number for `pselect6`
#define __NR_ppoll                  309 ///< System-call number for `ppoll`
#define __NR_unshare                310 ///< System-call number for `unshare`
#define __NR_set_robust_list        311 ///< System-call number for `set_robust_list`
#define __NR_get_robust_list        312 ///< System-call number for `get_robust_list`
#define __NR_splice                 313 ///< System-call number for `splice`
#define __NR_sync_file_range        314 ///< System-call number for `sync_file_range`
#define __NR_tee                    315 ///< System-call number for `tee`
#define __NR_vmsplice               316 ///< System-call number for `vmsplice`
#define __NR_move_pages             317 ///< System-call number for `move_pages`
#define __NR_getcpu                 318 ///< System-call number for `getcpu`
#define __NR_epoll_pwait            319 ///< System-call number for `epoll_pwait`
#define __NR_utimensat              320 ///< System-call number for `utimensat`
#define __NR_signalfd               321 ///< System-call number for `signalfd`
#define __NR_timerfd_create         322 ///< System-call number for `timerfd_create`
#define __NR_eventfd                323 ///< System-call number for `eventfd`
#define __NR_fallocate              324 ///< System-call number for `fallocate`
#define __NR_timerfd_settime        325 ///< System-call number for `timerfd_settime`
#define __NR_timerfd_gettime        326 ///< System-call number for `timerfd_gettime`
#define __NR_signalfd4              327 ///< System-call number for `signalfd4`
#define __NR_eventfd2               328 ///< System-call number for `eventfd2`
#define __NR_epoll_create1          329 ///< System-call number for `epoll_create1`
#define __NR_dup3                   330 ///< System-call number for `dup3`
#define __NR_pipe2                  331 ///< System-call number for `pipe2`
#define __NR_inotify_init1          332 ///< System-call number for `inotify_init1`
#define __NR_preadv                 333 ///< System-call number for `preadv`
#define __NR_pwritev                334 ///< System-call number for `pwritev`
#define __NR_rt_tgsigqueueinfo      335 ///< System-call number for `rt_tgsigqueueinfo`
#define __NR_perf_event_open        336 ///< System-call number for `perf_event_open`
#define __NR_recvmmsg               337 ///< System-call number for `recvmmsg`
#define __NR_fanotify_init          338 ///< System-call number for `fanotify_init`
#define __NR_fanotify_mark          339 ///< System-call number for `fanotify_mark`
#define __NR_prlimit64              340 ///< System-call number for `prlimit64`
#define __NR_name_to_handle_at      341 ///< System-call number for `name_to_handle_at`
#define __NR_open_by_handle_at      342 ///< System-call number for `open_by_handle_at`
#define __NR_clock_adjtime          343 ///< System-call number for `clock_adjtime`
#define __NR_syncfs                 344 ///< System-call number for `syncfs`
#define __NR_sendmmsg               345 ///< System-call number for `sendmmsg`
#define __NR_setns                  346 ///< System-call number for `setns`
#define __NR_process_vm_readv       347 ///< System-call number for `process_vm_readv`
#define __NR_process_vm_writev      348 ///< System-call number for `process_vm_writev`
#define __NR_kcmp                   349 ///< System-call number for `kcmp`
#define __NR_finit_module           350 ///< System-call number for `finit_module`
#define __NR_sched_setattr          351 ///< System-call number for `sched_setattr`
#define __NR_sched_getattr          352 ///< System-call number for `sched_getattr`
#define __NR_renameat2              353 ///< System-call number for `renameat2`
#define __NR_seccomp                354 ///< System-call number for `seccomp`
#define __NR_getrandom              355 ///< System-call number for `getrandom`
#define __NR_memfd_create           356 ///< System-call number for `memfd_create`
#define __NR_bpf                    357 ///< System-call number for `bpf`
#define __NR_execveat               358 ///< System-call number for `execveat`
#define __NR_socket                 359 ///< System-call number for `socket`
#define __NR_socketpair             360 ///< System-call number for `socketpair`
#define __NR_bind                   361 ///< System-call number for `bind`
#define __NR_connect                362 ///< System-call number for `connect`
#define __NR_listen                 363 ///< System-call number for `listen`
#define __NR_accept4                364 ///< System-call number for `accept4`
#define __NR_getsockopt             365 ///< System-call number for `getsockopt`
#define __NR_setsockopt             366 ///< System-call number for `setsockopt`
#define __NR_getsockname            367 ///< System-call number for `getsockname`
#define __NR_getpeername            368 ///< System-call number for `getpeername`
#define __NR_sendto                 369 ///< System-call number for `sendto`
#define __NR_sendmsg                370 ///< System-call number for `sendmsg`
#define __NR_recvfrom               371 ///< System-call number for `recvfrom`
#define __NR_recvmsg                372 ///< System-call number for `recvmsg`
#define __NR_shutdown               373 ///< System-call number for `shutdown`
#define __NR_userfaultfd            374 ///< System-call number for `userfaultfd`
#define __NR_membarrier             375 ///< System-call number for `membarrier`
#define __NR_mlock2                 376 ///< System-call number for `mlock2`
#define __NR_copy_file_range        377 ///< System-call number for `copy_file_range`
#define __NR_preadv2                378 ///< System-call number for `preadv2`
#define __NR_pwritev2               379 ///< System-call number for `pwritev2`
#define __NR_statx                  383 ///< System-call number for `statx`
#define __NR_arch_prctl             384 ///< System-call number for `arch_prctl`
#define __NR_io_pgetevents          385 ///< System-call number for `io_pgetevents`
#define __NR_rseq                   386 ///< System-call number for `rseq`
#define __NR_waitperiod             387 ///< System-call number for `waitperiod`
#define __NR_msgctl                 388 ///<  System-call number for `msgctl`
#define __NR_msgget                 389 ///<  System-call number for `msgget`
#define __NR_msgrcv                 390 ///<  System-call number for `msgrcv`
#define __NR_msgsnd                 391 ///<  System-call number for `msgsnd`
#define __NR_semctl                 392 ///<  System-call number for `semctl`
#define __NR_semget                 393 ///<  System-call number for `semget`
#define __NR_semop                  394 ///<  System-call number for `semop`
#define __NR_shmat                  395 ///<  System-call number for `shmat`
#define __NR_shmctl                 396 ///<  System-call number for `shmctl`
#define __NR_shmdt                  397 ///<  System-call number for `shmdt`
#define __NR_shmget                 398 ///<  System-call number for `shmget`
#define SYSCALL_NUMBER              399 ///< The total number of system-calls.

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

// Few things about what follows:
//
// 1. The symbol "=", is a a constraint modifier, and it means that the operand
//    is write-only, meaning that the previous value is discarded and replaced
//    by output data.
//
// 2. Using "0" here specifies that the input is read from a variable which also
//    serves as an output, the 0-th output variable in this case.
//
//

/// @brief Heart of the code that calls a system call with 0 parameters.
#define __inline_syscall_0(res, name) \
    __asm__ __volatile__("int $0x80"  \
                         : "=a"(res)  \
                         : "0"(__NR_##name))

/// @brief Heart of the code that calls a system call with 1 parameter.
#define __inline_syscall_1(res, name, arg1)                                \
    __asm__ __volatile__("push %%ebx; movl %2,%%ebx; int $0x80; pop %%ebx" \
                         : "=a"(res)                                       \
                         : "0"(__NR_##name), "ri"(arg1)                    \
                         : "memory");

/// @brief Heart of the code that calls a system call with 2 parameters.
#define __inline_syscall_2(res, name, arg1, arg2)                          \
    __asm__ __volatile__("push %%ebx; movl %2,%%ebx; int $0x80; pop %%ebx" \
                         : "=a"(res)                                       \
                         : "0"(__NR_##name), "ri"(arg1), "c"(arg2)         \
                         : "memory");

/// @brief Heart of the code that calls a system call with 3 parameters.
#define __inline_syscall_3(res, name, arg1, arg2, arg3)                       \
    __asm__ __volatile__("push %%ebx; movl %2,%%ebx; int $0x80; pop %%ebx"    \
                         : "=a"(res)                                          \
                         : "0"(__NR_##name), "ri"(arg1), "c"(arg2), "d"(arg3) \
                         : "memory");

/// @brief Heart of the code that calls a system call with 4 parameters.
#define __inline_syscall_4(res, name, arg1, arg2, arg3, arg4)                            \
    __asm__ __volatile__("push %%ebx; movl %2,%%ebx; int $0x80; pop %%ebx"               \
                         : "=a"(res)                                                     \
                         : "0"(__NR_##name), "ri"(arg1), "c"(arg2), "d"(arg3), "S"(arg4) \
                         : "memory");

/// @brief Heart of the code that calls a system call with 5 parameters.
#define __inline_syscall_5(res, name, arg1, arg2, arg3, arg4, arg5)                                 \
    __asm__ __volatile__("push %%ebx; movl %2,%%ebx; movl %1,%%eax; "                               \
                         "int $0x80; pop %%ebx"                                                     \
                         : "=a"(res)                                                                \
                         : "i"(__NR_##name), "ri"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5) \
                         : "memory");
