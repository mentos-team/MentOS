/// @file proc_running.c
/// @brief Implementaiton of procr filesystem.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fs/procfs.h"

#include "process/process.h"
#include "string.h"
#include "libgen.h"
#include "stdio.h"
#include "sys/errno.h"
#include "io/debug.h"
#include "process/prio.h"

/// @brief Returns the character identifying the process state.
/// @details
///     R  Running
///     S  Sleeping in an interruptible wait
///     D  Waiting in uninterruptible disk sleep
///     Z  Zombie
///     T  Stopped
///     t  Tracing stop
///     X  Dead
static inline char __procr_get_task_state_char(int state)
{
    if (state == 0x00) // TASK_RUNNING
        return 'R';
    if (state == (1 << 0)) // TASK_INTERRUPTIBLE
        return 'S';
    if (state == (1 << 1)) // TASK_UNINTERRUPTIBLE
        return 'D';
    if (state == (1 << 2)) // TASK_STOPPED
        return 'T';
    if (state == (1 << 3)) // TASK_TRACED
        return 't';
    if (state == (1 << 4)) // EXIT_ZOMBIE
        return 'Z';
    if (state == (1 << 5)) // EXIT_DEAD
        return 'X';
    return '?';
}

/// @brief Returns the data for the `/proc/<PID>/cmdline` file.
/// @param buffer the buffer where the data should be placed.
/// @param bufsize the size of the buffer.
/// @param task the task associated with the `/proc/<PID>` folder.
/// @return size of the written data in buffer.
static inline ssize_t __procr_do_cmdline(char *buffer, size_t bufsize, task_struct *task)
{
    strcpy(buffer, task->name);
    return 1;
}

/// @brief Returns the data for the `/proc/<PID>/stat` file.
/// @param buffer the buffer where the data should be placed.
/// @param bufsize the size of the buffer.
/// @param task the task associated with the `/proc/<PID>` folder.
/// @return size of the written data in buffer.
static inline ssize_t __procr_do_stat(char *buffer, size_t bufsize, task_struct *task)
{
    //(1) pid  %d
    //     The process ID.
    //
    sprintf(buffer, "%d", task->pid);
    //(2) comm  %s
    //     The filename of the executable, in parentheses.
    //     Strings longer than TASK_COMM_LEN (16) characters (in‐
    //     cluding the terminating null byte) are silently trun‐
    //     cated.  This is visible whether or not the executable
    //     is swapped out.
    //
    sprintf(buffer, "%s (%s)", buffer, basename(task->name));
    //(3) state  %c
    //     One of the following characters, indicating process state:
    //      R  Running
    //      S  Sleeping in an interruptible wait
    //      D  Waiting in uninterruptible disk sleep
    //      Z  Zombie
    //      T  Stopped
    //      t  Tracing stop
    //      X  Dead
    sprintf(buffer, "%s %c", buffer, __procr_get_task_state_char(task->state));
    //(4) ppid  %d
    //     The PID of the parent of this process.
    //
    if (task->parent)
        sprintf(buffer, "%s %d", buffer, task->parent->pid);
    else
        strcat(buffer, " 0");
    //(5) TODO: pgrp  %d
    //      The process group ID of the process.
    //
    strcat(buffer, " 0");
    //(6) TODO: session  %d
    //      The session ID of the process.
    //
    strcat(buffer, " 0");
    //(7) TODO: tty_nr  %d
    //      The controlling terminal of the process.  (The minor
    //      device number is contained in the combination of bits
    //      31 to 20 and 7 to 0; the major device number is in bits
    //      15 to 8.)
    //
    strcat(buffer, " 0");
    //(8) TODO: tpgid  %d
    //      The ID of the foreground process group of the control‐
    //      ling terminal of the process.
    //
    strcat(buffer, " 0");
    //(9) TODO: flags  %u
    //      The kernel flags word of the process.  For bit mean‐
    //      ings, see the PF_* defines in the Linux kernel source
    //      file include/linux/sched.h.  Details depend on the ker‐
    //      nel version.
    //      The format for this field was %lu before Linux 2.6.
    //
    strcat(buffer, " 0");
    //(10) TODO: minflt  %lu
    //      The number of minor faults the process has made which
    //      have not required loading a memory page from disk.
    //
    strcat(buffer, " 0");
    //(11) TODO: cminflt  %lu
    //      The number of minor faults that the process's waited-
    //      for children have made.
    //
    strcat(buffer, " 0");
    //(12) TODO: majflt  %lu
    //      The number of major faults the process has made which
    //      have required loading a memory page from disk.
    //
    strcat(buffer, " 0");
    //(13) TODO: cmajflt  %lu
    //      The number of major faults that the process's waited-
    //      for children have made.
    //
    strcat(buffer, " 0");
    //(14) TODO: utime  %lu
    //      Amount of time that this process has been scheduled in
    //      user mode, measured in clock ticks (divide by
    //      sysconf(_SC_CLK_TCK)).  This includes guest time,
    //      guest_time (time spent running a virtual CPU, see be‐
    //      low), so that applications that are not aware of the
    //      guest time field do not lose that time from their cal‐
    //      culations.
    //
    strcat(buffer, " 0");
    //(15) TODO: stime  %lu
    //      Amount of time that this process has been scheduled in
    //      kernel mode, measured in clock ticks (divide by
    //      sysconf(_SC_CLK_TCK)).
    //
    strcat(buffer, " 0");
    //(16) TODO: cutime  %ld
    //      Amount of time that this process's waited-for children
    //      have been scheduled in user mode, measured in clock
    //      ticks (divide by sysconf(_SC_CLK_TCK)).  (See also
    //      times(2).)  This includes guest time, cguest_time (time
    //      spent running a virtual CPU, see below).
    //
    strcat(buffer, " 0");
    //(17) TODO: cstime  %ld
    //      Amount of time that this process's waited-for children
    //      have been scheduled in kernel mode, measured in clock
    //      ticks (divide by sysconf(_SC_CLK_TCK)).
    //
    strcat(buffer, " 0");
    //(18) priority  %ld
    //      (Explanation for Linux 2.6) For processes running a
    //      real-time scheduling policy (policy below; see
    //      sched_setscheduler(2)), this is the negated scheduling
    //      priority, minus one; that is, a number in the range -2
    //      to -100, corresponding to real-time priorities 1 to 99.
    //      For processes running under a non-real-time scheduling
    //      policy, this is the raw nice value (setpriority(2)) as
    //      represented in the kernel.  The kernel stores nice val‐
    //      ues as numbers in the range 0 (high) to 39 (low), cor‐
    //      responding to the user-visible nice range of -20 to 19.
    //
    //      Before Linux 2.6, this was a scaled value based on the
    //      scheduler weighting given to this process.
    //
    sprintf(buffer, "%s %ld", buffer, task->se.prio);
    //(19) nice  %ld
    //      The nice value (see setpriority(2)), a value in the
    //      range 19 (low priority) to -20 (high priority).
    //
    sprintf(buffer, "%s %ld", buffer, PRIO_TO_NICE(task->se.prio));
    //(20) TODO: num_threads  %ld
    //      Number of threads in this process (since Linux 2.6).
    //      Before kernel 2.6, this field was hard coded to 0 as a
    //      placeholder for an earlier removed field.
    //
    strcat(buffer, " 0");
    //(21) TODO: itrealvalue  %ld
    //      The time in jiffies before the next SIGALRM is sent to
    //      the process due to an interval timer.  Since kernel
    //      2.6.17, this field is no longer maintained, and is hard
    //      coded as 0.
    //
    strcat(buffer, " 0");
    //(22) starttime  %llu
    //      The time the process started after system boot.  In
    //      kernels before Linux 2.6, this value was expressed in
    //      jiffies.  Since Linux 2.6, the value is expressed in
    //      clock ticks (divide by sysconf(_SC_CLK_TCK)).
    //
    //      The format for this field was %lu before Linux 2.6.
    //
    sprintf(buffer, "%s %lu", buffer, task->se.exec_start);
    //(23) vsize  %lu
    //      Virtual memory size in bytes.
    //
    sprintf(buffer, "%s %lu", buffer, task->mm->total_vm);
    //(24) TODO: rss  %ld
    //      Resident Set Size: number of pages the process has in
    //      real memory.  This is just the pages which count toward
    //      text, data, or stack space.  This does not include
    //      pages which have not been demand-loaded in, or which
    //      are swapped out.  This value is inaccurate; see
    //      /proc/[pid]/statm below.
    //
    strcat(buffer, " 0");
    //(25) TODO: rsslim  %lu
    //      Current soft limit in bytes on the rss of the process;
    //      see the description of RLIMIT_RSS in getrlimit(2).
    //
    strcat(buffer, " 0");
    //(26) startcode  %lu  [PT]
    //      The address above which program text can run.
    //
    sprintf(buffer, "%s %lu", buffer, task->mm->start_code);
    //(27) endcode  %lu  [PT]
    //      The address below which program text can run.
    //
    sprintf(buffer, "%s %lu", buffer, task->mm->end_code);
    //(28) startstack  %lu  [PT]
    //      The address of the start (i.e., bottom) of the stack.
    //
    sprintf(buffer, "%s %lu", buffer, task->mm->start_stack);
    //(29) kstkesp  %lu  [PT]
    //      The current value of ESP (stack pointer), as found in
    //      the kernel stack page for the process.
    //
    sprintf(buffer, "%s %lu", buffer, task->thread.regs.useresp);
    //(30) kstkeip  %lu  [PT]
    //      The current EIP (instruction pointer).
    //
    sprintf(buffer, "%s %lu", buffer, task->thread.regs.eip);
    //(31) TODO: signal  %lu
    //      The bitmap of pending signals, displayed as a decimal
    //      number.  Obsolete, because it does not provide informa‐
    //      tion on real-time signals; use /proc/[pid]/status in‐
    //      stead.
    //
    strcat(buffer, " 0");
    //(32) TODO: blocked  %lu
    //      The bitmap of blocked signals, displayed as a decimal
    //      number.  Obsolete, because it does not provide informa‐
    //      tion on real-time signals; use /proc/[pid]/status in‐
    //      stead.
    //
    strcat(buffer, " 0");
    //(33) TODO: sigignore  %lu
    //      The bitmap of ignored signals, displayed as a decimal
    //      number.  Obsolete, because it does not provide informa‐
    //      tion on real-time signals; use /proc/[pid]/status in‐
    //      stead.
    //
    strcat(buffer, " 0");
    //(34) TODO: sigcatch  %lu
    //      The bitmap of caught signals, displayed as a decimal
    //      number.  Obsolete, because it does not provide informa‐
    //      tion on real-time signals; use /proc/[pid]/status in‐
    //      stead.
    //
    strcat(buffer, " 0");
    //(35) TODO: wchan  %lu  [PT]
    //      This is the "channel" in which the process is waiting.
    //      It is the address of a location in the kernel where the
    //      process is sleeping.  The corresponding symbolic name
    //      can be found in /proc/[pid]/wchan.
    //
    strcat(buffer, " 0");
    //(36) TODO: nswap  %lu
    //      Number of pages swapped (not maintained).
    //
    strcat(buffer, " 0");
    //(37) TODO: cnswap  %lu
    //      Cumulative nswap for child processes (not maintained).
    //
    strcat(buffer, " 0");
    //(38) TODO: exit_signal  %d  (since Linux 2.1.22)
    //      Signal to be sent to parent when we die.
    //
    strcat(buffer, " 0");
    //(39) TODO: processor  %d  (since Linux 2.2.8)
    //      CPU number last executed on.
    //
    strcat(buffer, " 0");
    //(40) TODO: rt_priority  %u  (since Linux 2.5.19)
    //      Real-time scheduling priority, a number in the range 1
    //      to 99 for processes scheduled under a real-time policy,
    //      or 0, for non-real-time processes (see
    //      sched_setscheduler(2)).
    //
    if (task->se.prio >= 100)
        strcat(buffer, " 0");
    else
        sprintf(buffer, "%s %u", buffer, task->se.prio);
    //(41) TODO: policy  %u  (since Linux 2.5.19)
    //      Scheduling policy (see sched_setscheduler(2)).  Decode
    //      using the SCHED_* constants in linux/sched.h.
    //      The format for this field was %lu before Linux 2.6.22.
    //
    strcat(buffer, " 0");
    //(42) TODO: delayacct_blkio_ticks  %llu  (since Linux 2.6.18)
    //      Aggregated block I/O delays, measured in clock ticks
    //      (centiseconds).
    //
    strcat(buffer, " 0");
    //(43) TODO: guest_time  %lu  (since Linux 2.6.24)
    //      Guest time of the process (time spent running a virtual
    //      CPU for a guest operating system), measured in clock
    //      ticks (divide by sysconf(_SC_CLK_TCK)).
    //
    strcat(buffer, " 0");
    //(44) TODO: cguest_time  %ld  (since Linux 2.6.24)
    //      Guest time of the process's children, measured in clock
    //      ticks (divide by sysconf(_SC_CLK_TCK)).
    //
    strcat(buffer, " 0");
    //(45) start_data  %lu  (since Linux 3.3)  [PT]
    //      Address above which program initialized and uninitial‐
    //      ized (BSS) data are placed.
    //
    sprintf(buffer, "%s %lu", buffer, task->mm->start_data);
    //(46) end_data  %lu  (since Linux 3.3)  [PT]
    //      Address below which program initialized and uninitial‐
    //      ized (BSS) data are placed.
    //
    sprintf(buffer, "%s %lu", buffer, task->mm->end_data);
    //(47) start_brk  %lu  (since Linux 3.3)  [PT]
    //      Address above which program heap can be expanded with
    //      brk(2).
    //
    sprintf(buffer, "%s %lu", buffer, task->mm->start_brk);
    //(48) arg_start  %lu  (since Linux 3.5)  [PT]
    //      Address above which program command-line arguments
    //      (argv) are placed.
    //
    sprintf(buffer, "%s %lu", buffer, task->mm->arg_start);
    //(49) arg_end  %lu  (since Linux 3.5)  [PT]
    //      Address below program command-line arguments (argv) are
    //      placed.
    //
    sprintf(buffer, "%s %lu", buffer, task->mm->arg_end);
    //(50) env_start  %lu  (since Linux 3.5)  [PT]
    //      Address above which program environment is placed.
    //
    sprintf(buffer, "%s %lu", buffer, task->mm->env_start);
    //(51) env_end  %lu  (since Linux 3.5)  [PT]
    //      Address below which program environment is placed.
    //
    sprintf(buffer, "%s %lu", buffer, task->mm->env_end);
    //(52) exit_code  %d  (since Linux 3.5)  [PT]
    //      The thread's exit status in the form reported by
    //      waitpid(2).
    sprintf(buffer, "%s %d\n", buffer, task->exit_code);
    return 1;
}

/// @brief Performs a read of files inside the `/proc/<PID>/` folder.
/// @param file is the `/proc/<PID>/` folder, thus, it should be a `proc_dir_entry_t` data.
/// @param buffer buffer where the read content must be placed.
/// @param offset offset from which we start reading from the file.
/// @param nbyte the number of bytes to read.
/// @return The number of bytes we read.
static inline ssize_t __procr_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte)
{
    if (file == NULL)
        return -EFAULT;
    // Get the entry.
    proc_dir_entry_t *entry = (proc_dir_entry_t *)file->device;
    if (entry == NULL)
        return -EFAULT;
    // Get the task.
    task_struct *task = (task_struct *)entry->data;
    if (task == NULL)
        return -EFAULT;
    // Prepare a support buffer.
    char support[BUFSIZ];
    memset(support, 0, BUFSIZ);
    // Call the specific function.
    if (strcmp(entry->name, "cmdline") == 0)
        __procr_do_cmdline(support, BUFSIZ, task);
    else if (strcmp(entry->name, "stat") == 0)
        __procr_do_stat(support, BUFSIZ, task);
    // Copmute the amounts of bytes we want (and can) read.
    ssize_t bytes_to_read = max(0, min(strlen(support) - offset, nbyte));
    // Perform the read.
    if (bytes_to_read > 0)
        memcpy(buffer, support + offset, bytes_to_read);
    return bytes_to_read;
}

/// Filesystem general operations.
static vfs_sys_operations_t procr_sys_operations = {
    .mkdir_f = NULL,
    .rmdir_f = NULL,
    .stat_f  = NULL
};

/// Filesystem file operations.
static vfs_file_operations_t procr_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = __procr_read,
    .write_f    = NULL,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL
};

int procr_create_entry_pid(task_struct *entry)
{
    char path[PATH_MAX];
    proc_dir_entry_t *proc_dir = NULL, *proc_entry = NULL;
    {
        // Create `/proc/[PID]`.
        sprintf(path, "%d", entry->pid);
        // Create the proc entry root directory.
        if ((proc_dir = proc_mkdir(path, NULL)) == NULL) {
            pr_err("[task: %d] Cannot create proc root directory `%s`.\n", entry->pid, path);
            return -ENOENT;
        }
        proc_dir->data = entry;
    }
    {
        // Create `/proc/[PID]/cmdline`.
        if ((proc_entry = proc_create_entry("cmdline", proc_dir)) == NULL) {
            pr_err("[task: %d] Cannot create proc entry `%s`.\n", entry->pid, path);
            return -ENOENT;
        }
        proc_entry->sys_operations = &procr_sys_operations;
        proc_entry->fs_operations  = &procr_fs_operations;
        proc_entry->data           = entry;
    }
    {
        // Create `/proc/[PID]/stat`.
        if ((proc_entry = proc_create_entry("stat", proc_dir)) == NULL) {
            pr_err("[task: %d] Cannot create proc entry `%s`.\n", entry->pid, path);
            return -ENOENT;
        }
        proc_entry->sys_operations = &procr_sys_operations;
        proc_entry->fs_operations  = &procr_fs_operations;
        proc_entry->data           = entry;
    }
    return 0;
}

int procr_destroy_entry_pid(task_struct *entry)
{
    // Turn the pid into string. The maximum pid is 32768, thus entry pid is at most 6 chars.
    char pid_str[6];
    sprintf(pid_str, "%d", entry->pid);
    // Get the root directory.
    proc_dir_entry_t *proc_dir = proc_dir_entry_get(pid_str, NULL);
    if (proc_dir == NULL) {
        pr_err("[task: %d] Cannot find proc root directory `%s`.\n", entry->pid, pid_str);
        return -ENOENT;
    }
    // Destroy `/proc/[PID]/cmdline`.
    if (proc_destroy_entry("cmdline", proc_dir)) {
        pr_err("[task: %d] Cannot destroy proc cmdline.\n", entry->pid);
        return -ENOENT;
    }
    // Destroy `/proc/[PID]/stat`.
    if (proc_destroy_entry("stat", proc_dir)) {
        pr_err("[task: %d] Cannot destroy proc stat.\n", entry->pid);
        return -ENOENT;
    }
    // Destroy `/proc/[PID]`.
    if (proc_rmdir(pid_str, NULL)) {
        pr_err("[task: %d] Cannot remove proc root directory `%s`.\n", entry->pid, pid_str);
        return -ENOENT;
    }
    return 0;
}
