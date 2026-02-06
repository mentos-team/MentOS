/// @file scheduler.c
/// @brief Scheduler structures and functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[SCHED ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "assert.h"
#include "descriptor_tables/tss.h"
#include "errno.h"
#include "fs/vfs.h"
#include "hardware/timer.h"
#include "process/pid_manager.h"
#include "process/prio.h"
#include "process/scheduler.h"
#include "process/scheduler_feedback.h"
#include "process/wait.h"
#include "strerror.h"
#include "system/panic.h"

/// @brief          Assembly function setting the kernel stack to jump into
///                 location in Ring 3 mode (USER mode).
/// @param location The location where to jump.
/// @param stack    The stack to use.
extern void enter_userspace(uintptr_t location, uintptr_t stack);

/// The list of processes.
runqueue_t runqueue;

// Definition of the global init process pointer
task_struct *init_process = NULL;

void scheduler_initialize(void)
{
    // Initialize the runqueue list of tasks.
    list_head_init(&runqueue.queue);
    // Initialize the PID manager.
    pid_manager_init();
    // Reset the current task.
    runqueue.curr       = NULL;
    // Reset the number of active tasks.
    runqueue.num_active = 0;
}

task_struct *scheduler_get_current_process(void) { return runqueue.curr; }

time_t scheduler_get_maximum_vruntime(void)
{
    time_t vruntime = 0;
    task_struct *entry;
    list_for_each_decl (it, &runqueue.queue) {
        // Check if we reached the head of list_head, and skip it.
        if (it == &runqueue.queue) {
            continue;
        }
        // Get the current entry.
        entry = list_entry(it, task_struct, run_list);
        // Skip the process if it is a periodic one, we are issued to skip
        // periodic tasks, and the entry is not a periodic task under
        // analysis.
        if (entry->se.is_periodic && !entry->se.is_under_analysis) {
            continue;
        }
        if (entry->se.vruntime > vruntime) {
            vruntime = entry->se.vruntime;
        }
    }
    return vruntime;
}

size_t scheduler_get_active_processes(void) { return runqueue.num_active; }

task_struct *scheduler_get_running_process(pid_t pid)
{
    task_struct *entry;
    list_for_each_decl (it, &runqueue.queue) {
        entry = list_entry(it, task_struct, run_list);
        if (entry->pid == pid) {
            return entry;
        }
    }
    return NULL;
}

void scheduler_enqueue_task(task_struct *process)
{
    assert(process && "Received a NULL process.");
    // If current_process is NULL, then process is the current process.
    if (runqueue.curr == NULL) {
        runqueue.curr = process;
    }
    // Add the new process at the end.
    list_head_insert_before(&process->run_list, &runqueue.queue);
    // Increment the number of active processes.
    ++runqueue.num_active;

#ifdef ENABLE_SCHEDULER_FEEDBACK
    scheduler_feedback_task_add(process);
#endif
}

void scheduler_dequeue_task(task_struct *process)
{
    assert(process && "Received a NULL process.");
    // Delete the process from the list of running processes.
    list_head_remove(&process->run_list);
    // Decrement the number of active processes.
    --runqueue.num_active;
    if (process->se.is_periodic) {
        runqueue.num_periodic--;
    }

#ifdef ENABLE_SCHEDULER_FEEDBACK
    scheduler_feedback_task_remove(process->pid);
#endif
}

void scheduler_run(pt_regs_t *f)
{
    // Check if there is a running process.
    if (runqueue.curr == NULL) {
        return;
    }

    task_struct *next = NULL;

    // Update the context of the current process.
    scheduler_store_context(f, runqueue.curr);

    // We check the existence of pending signals every time we finish
    // handling an interrupt or an exception.
    if (!do_signal(f)) {
#if 1
        if (runqueue.curr->state == EXIT_ZOMBIE) {
            //==== Handle Zombies =================================================
            //pr_debug("Handle zombie %d\n", runqueue.curr->pid);
            // get the next process after the current one
            list_head_t *nNode = runqueue.curr->run_list.next;
            // check if we reached the head of list_head_t
            if (nNode == &runqueue.queue) {
                nNode = nNode->next;
            }
            // get the task_struct
            next = list_entry(nNode, task_struct, run_list);
            // Remove the zombie task.
            scheduler_dequeue_task(runqueue.curr);
            assert(next && "No valid task selected after removing ZOMBIE.");
            //=====================================================================
        } else {
#endif
            //==== Scheduling =====================================================
            // If we are currently executing a periodic process, and this process
            //  has yet to complete, keep executing it.
#ifdef SCHEDULER_EDF
            if (runqueue.curr->se.is_periodic)
                if (!runqueue.curr->se.executed)
                    return;
#endif
            // Pointer to the next process to be executed.
            next = scheduler_pick_next_task(&runqueue);
            //=====================================================================
        }
        // Check if the next and current processes are different.
        if (next != runqueue.curr) {
            // Copy into Kernel stack the next process's context.
            scheduler_restore_context(next, f);
        }
    }
    //==========================================================================
}

void scheduler_store_context(pt_regs_t *f, task_struct *process)
{
    // Store the registers.
    process->thread.regs = *f;
}

void scheduler_restore_context(task_struct *process, pt_regs_t *f)
{
    // Switch to the next process.
    runqueue.curr = process;
    // Restore the registers.
    *f            = process->thread.regs;
    // CRITICAL: Memory barrier to prevent compiler from reordering the page directory
    // switch before the above memory writes. In Release mode, the compiler can
    // reorder operations, which would cause us to switch page directories BEFORE
    // restoring the register context. This leads to immediate faults on process switch.
    __asm__ __volatile__("" ::: "memory");
    // TODO(enrico): Explain paging switch (ring 0 doesn't need page switching)
    // Switch to process page directory
    paging_switch_pgd(process->mm->pgd);
}

void scheduler_enter_user_jmp(uintptr_t location, uintptr_t stack)
{
    // Reset stack pointer for kernel.
    tss_set_stack(0x10, initial_esp);

    // update start execution time.
    runqueue.curr->se.start_runtime = timer_get_ticks();

    // last context switch time.
    runqueue.curr->se.exec_start = timer_get_ticks();

    // Jump in location.
    enter_userspace(location, stack);
}

int is_orphaned_pgrp(pid_t pgid)
{
    pid_t sid = 0;

    // Obtain SID of the group from a member
    list_for_each_decl (it, &runqueue.queue) {
        task_struct *task = list_entry(it, task_struct, run_list);
        if (task->pgid == pgid) {
            sid = task->sid;
            break;
        }
    }

    // Check if the process leader of the session is alive
    list_for_each_decl (it, &runqueue.queue) {
        task_struct *task = list_entry(it, task_struct, run_list);
        if (task->pid == sid) {
            return 0;
        }
    }

    return 1;
}

pid_t sys_getpid(void)
{
    // Ensure there is a running process in the runqueue.
    assert(runqueue.curr && "There is no currently running process.");

    // Return the process identifer of the process.
    return runqueue.curr->pid;
}

pid_t sys_getsid(pid_t pid)
{
    // If pid == 0, return the SID of the calling process.
    if (pid == 0) {
        // Ensure there is a running process in the runqueue.
        assert(runqueue.curr && "There is no currently running process.");
        // Return the session ID of the current process.
        return runqueue.curr->sid;
    }

    // If pid != 0, search for the process with the specified PID.
    list_for_each_decl (it, &runqueue.queue) {
        task_struct *task = list_entry(it, task_struct, run_list);
        if (task->pid == pid) {
            // Check if the current process belongs to the same session.
            if (runqueue.curr->sid != task->sid) {
                pr_debug(
                    "Access denied: Process %d is not in the same session as "
                    "the caller.",
                    pid);
                return -EPERM;
            }
            // Return the session ID of the target process.
            return task->sid;
        }
    }

    // If no matching process was found, return -ESRCH (No such process)
    pr_debug("No process with PID %d found in the runqueue.", pid);
    return -ESRCH;
}

pid_t sys_setsid(void)
{
    // Ensure there is a running process in the runqueue.
    assert(runqueue.curr && "There is no currently running process.");

    pid_t current_pid = runqueue.curr->pid;

    // Check if the process is already a session leader.
    if (runqueue.curr->sid == current_pid) {
        pr_debug("Process %d is already a session leader.", current_pid);
        return -EPERM;
    }

    // Assign the session ID and process group ID to the current process's PID.
    runqueue.curr->sid  = current_pid;
    runqueue.curr->pgid = current_pid;

    // Return the new session ID.
    return runqueue.curr->sid;
}

pid_t sys_getpgid(pid_t pid)
{
    task_struct *task = NULL;

    // If pid is 0, get the current running process.
    if (pid == 0) {
        task = runqueue.curr;
    }
    // Otherwise, fetch the process corresponding to the provided pid.
    else {
        task = scheduler_get_running_process(pid);
    }

    // If the task exists, return its process group ID (pgid).
    if (task != NULL) {
        return task->pgid;
    }

    // If no task was found, return an error code (-ESRCH) to indicate "no such
    // process".
    return -ESRCH;
}

int sys_setpgid(pid_t pid, pid_t pgid)
{
    task_struct *task = NULL;

    // If pid is 0, get the current running process.
    if (pid == 0) {
        task = runqueue.curr;
    }
    // Otherwise, fetch the process corresponding to the provided pid.
    else {
        task = scheduler_get_running_process(pid);
    }

    // Check if the task was found.
    if (task == NULL) {
        pr_err("Failed to find process with PID %d.", pid);
        return -ESRCH;
    }

    // If the process is a session leader, it cannot change its process group.
    if (task->pgid == task->pid) {
        pr_debug(
            "Process %d is already a session leader and cannot change its "
            "process group.",
            task->pid);
        return -EPERM;
    }

    // Set the new process group ID.
    task->pgid = pgid;

    pr_debug("Process %d assigned to process group %d.", task->pid, pgid);

    return 0;
}

/// Returns the attributes of the runnign process.
#define RETURN_PROCESS_ATTR_OR_EPERM(attr) \
    if (runqueue.curr) {                   \
        return runqueue.curr->attr;        \
    }                                      \
    return -EPERM;

uid_t sys_getuid(void) { RETURN_PROCESS_ATTR_OR_EPERM(ruid); }
uid_t sys_geteuid(void) { RETURN_PROCESS_ATTR_OR_EPERM(uid); }

gid_t sys_getgid(void) { RETURN_PROCESS_ATTR_OR_EPERM(rgid); }
gid_t sys_getegid(void) { RETURN_PROCESS_ATTR_OR_EPERM(gid); }

/// Checks the given ID.
#define FAIL_ON_INV_ID(id) \
    if ((id) < 0) {        \
        return -EINVAL;    \
    }

/// Checks the ID, and if there is a running process.
#define FAIL_ON_INV_ID_OR_PROC(id) \
    FAIL_ON_INV_ID(id)             \
    if (!runqueue.curr) {          \
        return -EPERM;             \
    }

/// If the process is ROOT, set the attribute and return 0.
#define IF_PRIVILEGED_SET_ALL_AND_RETURN(attr)               \
    if (runqueue.curr->uid == 0) {                           \
        runqueue.curr->r##attr = runqueue.curr->attr = attr; \
        return 0;                                            \
    }

/// Checks the attributes, resets them, and returns 0.
#define IF_RESET_SET_AND_RETURN(attr)       \
    if (runqueue.curr->r##attr == (attr)) { \
        runqueue.curr->attr = attr;         \
        return 0;                           \
    }

/// If the process is ROOT set the attribute, otherwise return failure.
#define SET_IF_PRIVILEGED_OR_FAIL(attr) \
    if (runqueue.curr->uid == 0) {      \
        runqueue.curr->attr = attr;     \
    } else {                            \
        return -EPERM;                  \
    }

int sys_setuid(uid_t uid)
{
    FAIL_ON_INV_ID_OR_PROC(uid);
    IF_PRIVILEGED_SET_ALL_AND_RETURN(uid);
    IF_RESET_SET_AND_RETURN(uid);
    return -EPERM;
}

int sys_setgid(gid_t gid)
{
    FAIL_ON_INV_ID_OR_PROC(gid);
    IF_PRIVILEGED_SET_ALL_AND_RETURN(gid);
    IF_RESET_SET_AND_RETURN(gid);
    return -EPERM;
}

int sys_setreuid(uid_t ruid, uid_t euid)
{
    if (!runqueue.curr) {
        return -EPERM;
    }

    if (euid != -1) {
        FAIL_ON_INV_ID(euid);
        // Privileged or reset?
        if ((runqueue.curr->uid == 0) || (runqueue.curr->ruid == euid)) {
            runqueue.curr->uid = euid;
        } else {
            return -EPERM;
        }
    }

    if (ruid != -1) {
        FAIL_ON_INV_ID(ruid);
        SET_IF_PRIVILEGED_OR_FAIL(ruid);
    }
    return 0;
}

int sys_setregid(gid_t rgid, gid_t egid)
{
    if (!runqueue.curr) {
        return -EPERM;
    }

    if (egid != -1) {
        FAIL_ON_INV_ID(rgid);
        // Privileged or reset?
        if ((runqueue.curr->uid == 0) || (runqueue.curr->rgid == egid)) {
            runqueue.curr->gid = egid;
        } else {
            return -EPERM;
        }
    }

    if (rgid != -1) {
        FAIL_ON_INV_ID(rgid);
        SET_IF_PRIVILEGED_OR_FAIL(rgid);
    }
    return 0;
}

pid_t sys_getppid(void)
{
    // Get the current task.
    if (runqueue.curr && runqueue.curr->parent) {
        return runqueue.curr->parent->pid;
    }
    return -EPERM;
}

int sys_nice(int increment)
{
    // Get the current task.
    if (runqueue.curr == NULL) {
        kernel_panic("There is no current process!");
    }

    if (increment < -40) {
        increment = -40;
    }
    if (increment > 40) {
        increment = 40;
    }

    int newNice = PRIO_TO_NICE(runqueue.curr->se.prio) + increment;
    pr_debug("New nice value would be : %d\n", newNice);

    if (newNice < MIN_NICE) {
        newNice = MIN_NICE;
    }
    if (newNice > MAX_NICE) {
        newNice = MAX_NICE;
    }

    if (PRIO_TO_NICE(runqueue.curr->se.prio) != newNice && newNice >= MIN_NICE && newNice <= MAX_NICE) {
        runqueue.curr->se.prio = NICE_TO_PRIO(newNice);
    }
    int actualNice = PRIO_TO_NICE(runqueue.curr->se.prio);

    pr_debug("Actual new nice value is: %d\n", actualNice);

    return actualNice;
}

pid_t sys_waitpid(pid_t pid, int *status, int options)
{
    // Ensure there is a running process in the runqueue.
    assert(runqueue.curr && "There is no currently running process.");

    // Validate the PID argument.
    // PIDs < -1 (process groups) and 0 (current process group) are not supported.
    if (pid < -1 || pid == 0) {
        return -ESRCH;
    }

    // A process cannot wait for itself.
    if (pid == runqueue.curr->pid) {
        return -ECHILD;
    }

    // Validate the `options` argument.
    // Supported options are WNOHANG and WUNTRACED; any other value is invalid
    if (options & ~(WNOHANG | WUNTRACED)) {
        pr_err("Invalid options: 0x%X\n", options);
        return -EINVAL;
    }

    // Check if the current process has any children.
    if (list_head_empty(&runqueue.curr->children)) {
        return -ECHILD;
    }

    // Iterate through the children of the current process.
    list_for_each_safe_decl(it, store, &runqueue.curr->children)
    {
        // Get the task_struct for the current child.
        task_struct *child = list_entry(it, task_struct, sibling);
        if (child == NULL) {
            continue;
        }

        // If the child is not in a zombie state, keep searching.
        if (child->state != EXIT_ZOMBIE) {
            continue;
        }

        // If a specific PID is provided, skip children with different PIDs.
        if ((pid > 1) && (child->pid != pid)) {
            continue;
        }

        // Prepare to return the child's PID and status
        pid_t child_pid = child->pid;
        if (status != NULL) {
            *status = child->exit_code;
        }

        // Clean up the child process's resources.
        pid_manager_mark_free(child->pid); // Free the PID.
        vfs_destroy_task(child);           // Finalize VFS structures.
        list_head_remove(&child->sibling); // Remove from parent's child list.
        scheduler_dequeue_task(child);     // Remove from the scheduler.
        kmem_cache_free(child);            // Free the `task_struct`.

        pr_debug("Process %d cleaned up child process %d.\n", runqueue.curr->pid, child_pid);

        // Return the PID of the cleaned-up child.
        return child_pid;
    }

    // No eligible child process was found.
    return 0;
}

void do_exit(int exit_code)
{
    // Ensure there is a running process in the runqueue.
    assert(runqueue.curr && "There is no currently running process.");

    // Get the process.
    if (runqueue.curr == init_process) {
        kernel_panic("Init process cannot call sys_exit!");
    }

    // Set the termination code of the process.
    runqueue.curr->exit_code = exit_code;
    // Set the state of the process to zombie.
    runqueue.curr->state     = EXIT_ZOMBIE;
    // Send a SIGCHLD to the parent process.
    if (runqueue.curr->parent) {
        int ret = sys_kill(runqueue.curr->parent->pid, SIGCHLD);
        if (ret == -1) {
            pr_err(
                "[%d] %5d failed sending signal %d : %s\n", ret, runqueue.curr->parent->pid, SIGCHLD, strerror(errno));
        }
    }

    // If it has children, then init process has to take care of them.
    if (!list_head_empty(&runqueue.curr->children)) {
        pr_debug(
            "Moving children of %s(%d) to init(%d): {\n", runqueue.curr->name, runqueue.curr->pid, init_process->pid);
        // Change the parent.
        pr_debug("Moving children (%d): {\n", init_process->pid);
        list_for_each_decl (it, &runqueue.curr->children) {
            task_struct *entry = list_entry(it, task_struct, sibling);
            pr_debug("    [%d] %s\n", entry->pid, entry->name);
            entry->parent = init_process;
        }
        pr_debug("}\n");
        // Plug the list of children.
        list_head_append(&init_process->children, &runqueue.curr->children);
        // Print the list of children.
        pr_debug("New list of init children (%d): {\n", init_process->pid);
        list_for_each_decl (it, &init_process->children) {
            task_struct *entry = list_entry(it, task_struct, sibling);
            pr_debug("    [%d] %s\n", entry->pid, entry->name);
        }
        pr_debug("}\n");
    }
    // Free the space occupied by the stack.
    mm_destroy(runqueue.curr->mm);
    // Debugging message.
    pr_debug("Process %d exited with value %d\n", runqueue.curr->pid, exit_code);
}

void sys_exit(int exit_code) { do_exit(exit_code << 8); }

int sys_sched_setparam(pid_t pid, const sched_param_t *param)
{
    // Iter over the runqueue to find the task
    list_for_each_decl (it, &runqueue.queue) {
        task_struct *entry = list_entry(it, task_struct, run_list);
        if (entry->pid == pid) {
            if (!entry->se.is_periodic && param->is_periodic) {
                runqueue.num_periodic++;
            } else if (entry->se.is_periodic && !param->is_periodic) {
                runqueue.num_periodic--;
            }
            // Sets the parameters from param to the "se" struct parameters.
            entry->se.prio        = param->sched_priority;
            entry->se.period      = param->period;
            entry->se.arrivaltime = param->arrivaltime;
            entry->se.is_periodic = param->is_periodic;
            entry->se.deadline    = timer_get_ticks() + param->deadline;
            entry->se.next_period = timer_get_ticks();

            entry->se.is_under_analysis = true;
            entry->se.executed          = false;
            return 1;
        }
    }
    return -1;
}

int sys_sched_getparam(pid_t pid, sched_param_t *param)
{
    // Iter over the runqueue to find the task
    list_for_each_decl (it, &runqueue.queue) {
        task_struct *entry = list_entry(it, task_struct, run_list);
        if (entry->pid == pid) {
            //Sets the parameters from the "se" struct to param
            param->sched_priority = entry->se.prio;
            param->period         = entry->se.period;
            param->deadline       = entry->se.deadline;
            param->arrivaltime    = entry->se.arrivaltime;
            return 1;
        }
    }
    return -1;
}

/// @brief Performs the response time analysis for the current list of periodic
/// processes.
/// @return 1 if scheduling periodic processes is feasible, 0 otherwise.
static int __response_time_analysis(void)
{
    task_struct *entry;
    task_struct *previous;
    time_t r;
    time_t previous_r = 0;
    list_for_each_decl (it, &runqueue.queue) {
        // Get the curent entry in the list.
        entry = list_entry(it, task_struct, run_list);
        // If the process is not periodic we skip it.
        if (!entry->se.is_periodic) {
            continue;
        }
        // Put r equal to worst case exec because is the first point in time
        // that the task could possibly complete.
        r = entry->se.worst_case_exec, previous_r = 0;
        // The analysis can be completed either missing the deadline or reaching
        // a fixed point.
        while ((r < entry->se.deadline) && (r != previous_r)) {
            // Save the previous response time.
            previous_r = r;
            // Initialize response time.
            r          = entry->se.worst_case_exec;
            list_for_each_decl (it2, &runqueue.queue) {
                previous = list_entry(it2, task_struct, run_list);
                // Check the interferences of higher priority processes.
                if (previous->se.is_periodic && (previous->se.period < entry->se.period)) {
                    pr_debug(
                        "%d += (%.2f / %.2f) * %d\n", r, (double)previous_r, (double)previous->se.period,
                        previous->se.worst_case_exec);

                    // Update the response time.
                    r += (int)ceil((double)previous_r / (double)previous->se.period) * previous->se.worst_case_exec;

                    pr_debug("Response Time Analysis -> [%s] vs [%s] R = %d\n\n", entry->name, previous->name, r);
                }
            }
        }
        // Feasibility of scheduler is guaranteed if and only if response time
        // analysis is lower than deadline.
        if (r > entry->se.deadline) {
            return 1;
        }
    }
    return 0;
}

/// @brief Computes the total utilization factor.
/// @return the utilization factor.
static inline double __compute_utilization_factor(void)
{
    task_struct *entry;
    double U = 0;
    list_for_each_decl (it, &runqueue.queue) {
        // Get the entry.
        entry = list_entry(it, task_struct, run_list);
        // Sum the utilization factor of all periodic tasks.
        if (entry->se.is_periodic) {
            U += entry->se.utilization_factor;
        }
    }
    return U;
}

int sys_waitperiod(void)
{
    // Get the current process.
    task_struct *current = scheduler_get_current_process();
    // Check if there is actually a process running.
    if (current == NULL) {
        pr_emerg("There is no current process.\n");
        return -ESRCH;
    }
    // Check if the process calling the waitperiod function is a periodic process.
    if (!current->se.is_periodic) {
        pr_warning("An aperiodic task is calling `waitperiod`, ignoring...\n");
        return -EPERM;
    }
    // Get the current time.
    time_t current_time = timer_get_ticks();

    // Update the Worst Case Execution Time (WCET).
    time_t wcet = current_time - current->se.exec_start;
    if (current->se.worst_case_exec < wcet) {
        current->se.worst_case_exec = wcet;
    }
    // Update the utilization factor.
    current->se.utilization_factor = ((double)current->se.worst_case_exec / (double)current->se.period);
    // If the task is under analysis, we need to test if the process can be
    // placed with the other periodic tasks.
    if (current->se.is_under_analysis) {
        // Set the WCET as the total execution time of the process.
        current->se.worst_case_exec = current->se.sum_exec_runtime;
        // This will keep track if the process can be scheduled.
        bool_t is_not_schedulable   = false;
#if defined(SCHEDULER_EDF)
        // Compute the total utilization factor.
        double u = __compute_utilization_factor();
        // If the utilization factor is above 1, the process cannot be placed
        // with the other periodic processes.
        if (u > 1) {
            is_not_schedulable = true;
        }
        pr_warning("Utilization factor is : %.2f\n", u);
#elif defined(SCHEDULER_RM)
        // Compute the total utilization factor.
        double u    = __compute_utilization_factor();
        // Calculating Least Upper Bound of utilization factor. For large amount
        // of processes ulub asymptotically should reach ln(2).
        double ulub = (runqueue.num_periodic * (pow(2, (1.0 / runqueue.num_periodic)) - 1));
        // If the sum of utilization factor is bounded between ulub and 1 we
        // need to calculate the response time analysis for each process.
        if (u > 1) {
            is_not_schedulable = true;
        } else if (u <= ulub) {
            is_not_schedulable = false;
        } else {
            is_not_schedulable = __response_time_analysis();
        }
        pr_warning("Utilization factor is : %.2f, Least Upper Bound: %.2f\n", u, ulub);
#endif
        // If it is not schedulable, we need to tell it to the process.
        if (is_not_schedulable) {
            return -ENOTSCHEDULABLE;
        }
        // Otherwise, it is schedulable and thus it is not under analysis
        // anymore.
        current->se.is_under_analysis = false;
        // The task has been executed as non-periodic process so that his
        // deadline is not been updated by the scheduling algorithm of periodic
        // tasks. We need to update it manually.
        current->se.next_period       = current_time;
        current->se.deadline          = current_time + current->se.period;
    }
    // If the current time is ahead of the deadline, we need to print a warning.
    if (current_time > current->se.deadline) {
        pr_warning("%d > %d Missing deadline...\n", current_time, current->se.deadline);
    }
    // Tell the scheduler that we have executed the periodic process.
    current->se.executed = true;
    return 0;
}
