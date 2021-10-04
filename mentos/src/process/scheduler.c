///                MentOS, The Mentoring Operating system project
/// @file scheduler.c
/// @brief Scheduler structures and functions.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

/// Change the header.
#define __DEBUG_HEADER__ "[SCHED ]"

#include "assert.h"
#include "strerror.h"
#include "vfs.h"
#include "scheduler.h"
#include "tss.h"
#include "fpu.h"
#include "prio.h"
#include "wait.h"
#include "kheap.h"
#include "panic.h"
#include "debug.h"
#include "time.h"
#include "errno.h"
#include "list_head.h"
#include "paging.h"
#include "timer.h"
#include "math.h"
#include "stdio.h"

/// @brief          Assembly function setting the kernel stack to jump into
///                 location in Ring 3 mode (USER mode).
/// @param location The location where to jump.
/// @param stack    The stack to use.
extern void enter_userspace(uintptr_t location, uintptr_t stack);

/// The list of processes.
runqueue_t runqueue;

void scheduler_initialize()
{
    // Initialize the runqueue list of tasks.
    list_head_init(&runqueue.queue);
    // Reset the current task.
    runqueue.curr = NULL;
    // Reset the number of active tasks.
    runqueue.num_active = 0;
}

uint32_t scheduler_getpid(void)
{
    /// The current unused PID.
    static unsigned long int tid = 1;

    // Return the pid and increment.
    return tid++;
}

task_struct *scheduler_get_current_process()
{
    return runqueue.curr;
}

time_t scheduler_get_maximum_vruntime()
{
    time_t vruntime = 0;
    task_struct *entry;
    list_for_each_decl(it, &runqueue.queue)
    {
        // Check if we reached the head of list_head, and skip it.
        if (it == &runqueue.queue)
            continue;
        // Get the current entry.
        entry = list_entry(it, task_struct, run_list);
        // Skip the process if it is a periodic one, we are issued to skip
        // periodic tasks, and the entry is not a periodic task under
        // analysis.
        if (entry->se.is_periodic && !entry->se.is_under_analysis)
            continue;
        if (entry->se.vruntime > vruntime)
            vruntime = entry->se.vruntime;
    }
    return vruntime;
}

size_t scheduler_get_active_processes()
{
    return runqueue.num_active;
}

task_struct *scheduler_get_running_process(pid_t pid)
{
    task_struct *entry;
    list_for_each_decl(it, &runqueue.queue)
    {
        entry = list_entry(it, task_struct, run_list);
        if (entry->pid == pid)
            return entry;
    }
    return NULL;
}

void scheduler_enqueue_task(task_struct *process)
{
    // If current_process is NULL, then process is the current process.
    if (runqueue.curr == NULL) {
        runqueue.curr = process;
    }
    // Add the new process at the end.
    list_head_add_tail(&process->run_list, &runqueue.queue);
    // Increment the number of active processes.
    ++runqueue.num_active;
}

void scheduler_dequeue_task(task_struct *process)
{
    // Delete the process from the list of running processes.
    list_head_del(&process->run_list);
    // Decrement the number of active processes.
    --runqueue.num_active;
    if (process->se.is_periodic)
        runqueue.num_periodic--;
}

void scheduler_run(pt_regs *f)
{
    // Check if there is a running process.
    if (runqueue.curr == NULL)
        return;

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
            list_head *nNode = runqueue.curr->run_list.next;
            // check if we reached the head of list_head
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

void scheduler_store_context(pt_regs *f, task_struct *process)
{
    // Store the registers.
    process->thread.regs = *f;
}

void scheduler_restore_context(task_struct *process, pt_regs *f)
{
    // Switch to the next process.
    runqueue.curr = process;
    // Restore the registers.
    *f = process->thread.regs;
    // TODO: Explain paging switch (ring 0 doesn't need page switching)
    // Switch to process page directory
    paging_switch_directory_va(process->mm->pgd);
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

/// @brief Awakens a sleeping process.
/// @param process The process that should be awakened
/// @param mode The type of wait (TASK_INTERRUPTIBLE or TASK_UNINTERRUPTIBLE).
/// @param sync Specifies if the wakeup should be synchronous.
/// @return 1 on success, 0 on failure.
static inline int try_to_wake_up(task_struct *process, int mode, int sync)
{
    // Only tasks in the state TASK_UNINTERRUPTIBLE can be woke up
    if (process->state == TASK_UNINTERRUPTIBLE || process->state == TASK_STOPPED) {
        //TODO: Recalc task priority
        process->state = TASK_RUNNING;
        return 1;
    }
    return 0;
}

int default_wake_function(wait_queue_entry_t *wait, unsigned mode, int sync)
{
    task_struct *p = wait->task;
    return try_to_wake_up(p, mode, sync);
}

wait_queue_entry_t *sleep_on(wait_queue_head_t *wq)
{
    // Save the sleeping process registers state
    task_struct *sleeping_task = scheduler_get_current_process();

#if 0
    pt_regs* f = get_current_interrupt_stack_frame();
    scheduler_store_context(f, sleeping_task);

    // Select next process in the runqueue as the current, restore it's context,
    // we assume that the first process is init wich does not sleep (I hope).
    // This is necessary to make the scheduler_run() in syscall_handler work.
    task_struct *next = list_entry(runqueue.queue.next, task_struct, run_list);
    assert((next != sleeping_task) && "The next selected process in the runqueue is the sleeping process");
    scheduler_restore_context(next, f);
#endif

    // Stops task from runqueue making it unrunnable
    sleeping_task->state = TASK_UNINTERRUPTIBLE;

    // Add sleeping process to sleep wait queue
    wait_queue_entry_t *wait_entry = kmalloc(sizeof(struct wait_queue_entry_t));
    init_waitqueue_entry(wait_entry, sleeping_task);
    add_wait_queue(wq, wait_entry);

    return wait_entry;
}

int is_orphaned_pgrp(pid_t gid)
{
    pid_t sid = 0;

    // Obtain SID of the group from a member
    list_head *it;
    list_for_each (it, &runqueue.queue) {
        task_struct *task = list_entry(it, task_struct, run_list);
        if (task->gid == gid) {
            sid = task->sid;
            break;
        }
    }

    // Check if the process leader of the session is alive
    list_for_each (it, &runqueue.queue) {
        task_struct *task = list_entry(it, task_struct, run_list);
        if (task->pid == sid) {
            return 0;
        }
    }

    return 1;
}

pid_t sys_getpid()
{
    // Get the current task.
    if (runqueue.curr == NULL) {
        kernel_panic("There is no current process!");
    }

    // Return the process identifer of the process.
    return runqueue.curr->pid;
}

pid_t sys_getsid(pid_t pid)
{
    //If pid == 0 return SID of the calling process
    if (pid == 0) {
        if (runqueue.curr == NULL) {
            kernel_panic("There is no current process!");
        }
        // Return the session identifer of the process.
        return runqueue.curr->sid;
    }
    //If != 0 get SID of the specified process
    list_head *it;
    list_for_each (it, &runqueue.queue) {
        task_struct *task = list_entry(it, task_struct, run_list);
        if (task->pid == pid)
        {
            if(runqueue.curr->sid != task->sid)     
                return -EPERM;
            
            return task->sid;
        }
    }
    return -ESRCH;
}

pid_t sys_setsid()
{
    task_struct *task = runqueue.curr;
    if (task == NULL) {
        kernel_panic("There is no current process!");
    }
    if (task->sid == task->pid)
    {
        pr_debug("Process %d is already a session leader.", task->pid);
        return -EPERM;
    }

    task->sid = task->pid;
    task->gid = task->pid;

    return task->sid;
}

pid_t sys_getgid()
{
    task_struct *curr = runqueue.curr;
    if (curr == NULL) {
        kernel_panic("There is no current process!");
    }

    return curr->gid;
}

int sys_setgid(pid_t gid)
{
    task_struct *curr = runqueue.curr;
    if (curr == NULL) {
        kernel_panic("There is no current process!");
    }

    if (curr->gid == curr->pid)
        pr_debug("Process %d is already a session leader.", task->pid);

    curr->gid = curr->pid;
    return 0;
}

pid_t sys_getppid()
{
    // Get the current task.
    if (runqueue.curr == NULL) {
        kernel_panic("There is no current process!");
    }
    if (runqueue.curr->parent == NULL) {
        return 0;
    }

    // Return the parent process identifer of the process.
    return runqueue.curr->parent->pid;
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
    // Get the current task.
    if (runqueue.curr == NULL) {
        kernel_panic("There is no current process!");
    }

    /* For now we do not support waiting for processes inside the given
     * process group (pid < -1).
     */
    if ((pid < -1) || (pid == 0)) {
        return -ESRCH;
    }
    if (pid == runqueue.curr->pid) {
        return -ECHILD;
    }
    if (options != 0 && options != WNOHANG) {
        return -EINVAL;
    }
#if 0
    if (status == NULL) {
        return -EFAULT;
    }
#endif
    if (list_head_empty(&runqueue.curr->children)) {
        return -ECHILD;
    }
    list_head *it;
    list_for_each (it, &runqueue.curr->children) {
        task_struct *entry = list_entry(it, task_struct, sibling);
        if (entry == NULL) {
            continue;
        }
        if (entry->state != EXIT_ZOMBIE) {
            continue;
        }
        if ((pid > 1) && (entry->pid != pid)) {
            continue;
        }
        // Save the pid to return.
        pid_t ppid = entry->pid;
        // Save the state (TODO: Improve status set).
        if (status)
            (*status) = entry->state;
        // Finalize the VFS structures.
        vfs_destroy_task(entry);
        // Remove entry from children of parent.
        list_head_del(&entry->sibling);
        // Remove entry from the scheduling queue.
        scheduler_dequeue_task(entry);
        // Delete the task_struct.
        kmem_cache_free(entry);
        pr_debug("Process %d is freeing memory of process %d.\n", runqueue.curr->pid, ppid);
        return ppid;
    }
    return 0;
}

void sys_exit(int exit_code)
{
    // Get the current task.
    if (runqueue.curr == NULL) {
        kernel_panic("There is no current process!");
    }

    // Get the process.
    task_struct *init_proc = scheduler_get_running_process(1);
    if (runqueue.curr == init_proc) {
        kernel_panic("Init process cannot call sys_exit!");
    }

    // Set the termination code of the process.
    runqueue.curr->exit_code = (exit_code << 8) & 0xFF00;
    // Set the state of the process to zombie.
    runqueue.curr->state = EXIT_ZOMBIE;
    // Send a SIGCHLD to the parent process.
    if (runqueue.curr->parent) {
        int ret = sys_kill(runqueue.curr->parent->pid, SIGCHLD);
        if (ret == -1) {
            printf("[%d] %5d failed sending signal %d : %s\n", ret, runqueue.curr->parent->pid,
                   SIGCHLD, strerror(errno));
        }
    }

    // If it has children, then init process has to take care of them.
    if (!list_head_empty(&runqueue.curr->children)) {
        pr_debug("Moving children of %s(%d) to init(%d): {\n",
                 runqueue.curr->name, runqueue.curr->pid, init_proc->pid);
        // Change the parent.
        pr_debug("Moving children (%d): {\n", init_proc->pid);
        list_for_each_decl(it, &runqueue.curr->children)
        {
            task_struct *entry = list_entry(it, task_struct, sibling);
            pr_debug("    [%d] %s\n", entry->pid, entry->name);
            entry->parent = init_proc;
        }
        pr_debug("}\n");
        // Plug the list of children.
        list_head_merge(&init_proc->children, &runqueue.curr->children);
        // Print the list of children.
        pr_debug("New list of init children (%d): {\n", init_proc->pid);
        list_for_each_decl(it, &init_proc->children)
        {
            task_struct *entry = list_entry(it, task_struct, sibling);
            pr_debug("    [%d] %s\n", entry->pid, entry->name);
        }
        pr_debug("}\n");
    }
    // Free the space occupied by the stack.
    destroy_process_image(runqueue.curr->mm);
    // Debugging message.
    pr_debug("Process %d exited with value %d\n", runqueue.curr->pid, exit_code);
}

int sys_sched_setparam(pid_t pid, const sched_param_t *param)
{
    list_head *it;
    // Iter over the runqueue to find the task
    list_for_each (it, &runqueue.queue) {
        task_struct *entry = list_entry(it, task_struct, run_list);
        if (entry->pid == pid) {
            if (!entry->se.is_periodic && param->is_periodic)
                runqueue.num_periodic++;
            else if (entry->se.is_periodic && !param->is_periodic)
                runqueue.num_periodic--;
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
    list_head *it;
    // Iter over the runqueue to find the task
    list_for_each (it, &runqueue.queue) {
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

/// @brief Performs the response time analysis for the current list
///        of periodic processes.
/// @return 1 if scheduling periodic processes is feasable, 0 otherwise.
static int __response_time_analysis()
{
    task_struct *entry, *previous;
    time_t r, previous_r = 0;
    list_for_each_decl(it, &runqueue.queue)
    {
        entry = list_entry(it, task_struct, run_list);
        if (entry->se.is_periodic) {
            // Put r equal to worst case exec because is the first point in time that the task could possibly complete
            r          = entry->se.worst_case_exec;
            previous_r = 0;
            // The analysis can be completed either missing the deadline or reaching a fixed point
            while (r < entry->se.deadline && r != previous_r) {
                previous_r = r;
                r          = entry->se.worst_case_exec;
                list_for_each_decl(it2, &runqueue.queue)
                {
                    previous = list_entry(it2, task_struct, run_list);
                    // Check the interferences of higher priority processes
                    if (previous->se.is_periodic && previous->se.period < entry->se.period) {
                        r += (int)ceil((double)previous_r / (double)previous->se.period) * previous->se.worst_case_exec;
                        pr_debug("%d += (%.2f / %.2f) * %d\n", r, (double)previous_r, (double)previous->se.period, previous->se.worst_case_exec);
                        pr_debug("Response Time Analysis -> [%s]vs[%s] R = %d\n\n", entry->name, previous->name, r);
                    }
                }
            }
            // Feasibility of scheduler is guaranteed if and only if response time analysis is lower than deadline.
            if (r > entry->se.deadline)
                return 1;
        }
    }
    return 0;
}

int sys_waitperiod()
{
    if (runqueue.curr) {
        if (runqueue.curr->se.is_periodic) {
            // Update the Worst Case Execution Time (WCET).
            time_t wcet = timer_get_ticks() - runqueue.curr->se.exec_start;
            if (runqueue.curr->se.worst_case_exec < wcet)
                runqueue.curr->se.worst_case_exec = wcet;
            // Update thye utilization factor.
            runqueue.curr->se.utilization_factor = ((double)runqueue.curr->se.worst_case_exec / (double)runqueue.curr->se.period);

            // If the task is under analysis, we need to test if the process can be
            //  placed with the other periodic tasks.
            if (runqueue.curr->se.is_under_analysis) {
                runqueue.curr->se.worst_case_exec = runqueue.curr->se.sum_exec_runtime;
                bool_t is_not_schedulable         = false;
#if defined(SCHEDULER_EDF)
                double u = 0;
                task_struct *entry;
                list_for_each_decl(it, &runqueue.queue)
                {
                    entry = list_entry(it, task_struct, run_list);
                    // Sum the utilization factor of all periodic tasks.
                    if (entry->se.is_periodic)
                        u += entry->se.utilization_factor;
                }
                if (u > 1) {
                    is_not_schedulable = true;
                }
                pr_debug("utilization factor = %f\n", u);
#elif defined(SCHEDULER_RM)
                // Calculating least upper bound of utilization factor.
                // For large amount of processes ulub asymptotically should reach ln(2).
                double ulub = (runqueue.num_periodic * (pow(2, (1.0 / runqueue.num_periodic)) - 1));
                double u    = 0;
                task_struct *entry;
                list_for_each_decl(it, &runqueue.queue)
                {
                    entry = list_entry(it, task_struct, run_list);
                    // Sum the utilization factor of all periodic tasks.
                    if (entry->se.is_periodic)
                        u += entry->se.utilization_factor;
                }
                // If the sum of utilization factor is bounded between ulub and 1 we need to calculate
                // the response time analysis for each process.
                if (u > 1) {
                    is_not_schedulable = true;
                } else if (u <= ulub)
                    is_not_schedulable = false;
                else
                    is_not_schedulable = __response_time_analysis();
#endif
                // If it is not schedulable, we need to tell it to the process.
                if (is_not_schedulable)
                    return -ENOTSCHEDULABLE;

                // Otherwise, it is schedulable.
                runqueue.curr->se.is_under_analysis = false;

                // The task has been executed as non-periodic process so that his deadline is not been updated
                // by the scheduling algorithm of periodic tasks. We need to update it manually.
                runqueue.curr->se.next_period = timer_get_ticks();
                runqueue.curr->se.deadline    = timer_get_ticks() + runqueue.curr->se.period;
            }

            if (timer_get_ticks() > runqueue.curr->se.deadline)
                pr_warning("%d > %d Missing deadline...\n", timer_get_ticks(), runqueue.curr->se.deadline);

            // Tell the scheduler that we have executed the periodic process.
            runqueue.curr->se.executed = true;

        } else
            pr_warning("An aperiodic task is calling `waitperiod`, ignoring...\n");
        return 0;
    }
    return -ESRCH;
}