///                MentOS, The Mentoring Operating system project
/// @file scheduler.c
/// @brief Scheduler structures and functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "scheduler.h"
#include "tss.h"
#include "fpu.h"
#include "prio.h"
#include "wait.h"
#include "kheap.h"
#include "panic.h"
#include "debug.h"
#include "clock.h"
#include "errno.h"
#include "rbtree.h"
#include "stdlib.h"
#include "list_head.h"

/// @brief          Assembly function setting the kernel stack to jump into
///                 location in Ring 3 mode (USER mode).
/// @param location The location where to jump.
/// @param stack    The stack to use.
extern void enter_userspace(uintptr_t location, uintptr_t stack);

/// The list of processes.
runqueue_t runqueue;

uint32_t get_new_pid(void)
{
	/// The current unused PID.
	static unsigned long int tid = 1;

	// Return the pid and increment.
	return tid++;
}

task_struct *kernel_get_current_process()
{
	return runqueue.curr;
}

task_struct *kernel_get_running_process(pid_t pid)
{
	list_head *it;
	list_for_each (it, &runqueue.queue) {
		task_struct *entry = list_entry(it, task_struct, run_list);
		if (entry != NULL) {
			if (entry->pid == pid) {
				return entry;
			}
		}
	}
	return NULL;
}

size_t kernel_get_active_processes()
{
	return runqueue.num_active;
}

void kernel_initialize_scheduler()
{
	// Initialize the runqueue list of tasks.
	list_head_init(&runqueue.queue);
	// Reset the current task.
	runqueue.curr = NULL;
	// Reset the number of active tasks.
	runqueue.num_active = 0;
}

void enqueue_task(task_struct *process)
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

void dequeue_task(task_struct *process)
{
	// Delete the process from the list of running processes.
	list_head_del(&process->run_list);
	// Decrement the number of active processes.
	--runqueue.num_active;
}

void kernel_schedule(pt_regs *f)
{
	// Check if there is a running process.
	if (runqueue.curr == NULL) {
		return;
	}

	//==== Update Statistics ===================================================
	time_t delta_exec = get_millisecond() - runqueue.curr->se.exec_start;
	//	dbg_print("[%3d] %d = %d - %d\n", runqueue.curr->pid, delta_exec,
	//			  get_millisecond(), runqueue.curr->se.exec_start);
	// set the sum_exec_runtime
	runqueue.curr->se.sum_exec_runtime += delta_exec;
	//==========================================================================

	//==== Handle Zombies ======================================================
	task_struct *next_process = NULL;
	if (runqueue.curr->state == EXIT_ZOMBIE) {
		// get the next process after the current one
		list_head *nNode = runqueue.curr->run_list.next;
		// check if we reached the head of list_head
		if (nNode == &runqueue.queue)
			nNode = nNode->next;
		// get the task_struct
		next_process = list_entry(nNode, task_struct, run_list);
		// Remove the zombie task.
		dequeue_task(runqueue.curr);
	} else {
		//==== Scheduling ======================================================
		// Pointer to the next process to be executed.
		next_process = pick_next_task(&runqueue, delta_exec);
		//======================================================================
	}
	//==========================================================================

	// Print, for debugging purpose, data about the current process.
	if (runqueue.num_active > 2) {
		dbg_print("PID:%3d, PRIO:%3d, VRUNTIME:%9d, SUM_EXEC:%9d\n",
				  next_process->pid, next_process->se.prio,
				  next_process->se.vruntime, next_process->se.sum_exec_runtime);
	}

	//==== Context switch ======================================================
	// Update the context of the current process.
	update_context(f, runqueue.curr);
	// Check if the next and current processes are different.
	if (next_process != runqueue.curr) {
		// Copy into Kernel stack the next process's context.
		do_switch(next_process, f);
		runqueue.curr->se.sum_exec_runtime = get_millisecond();
		// Update the last context switch time of the next process.
		next_process->se.exec_start = get_millisecond();
	}
	//==========================================================================

	// Update the start execution time if it is executed for the first time
	if (next_process->se.start_runtime == 0)
		next_process->se.start_runtime = get_millisecond();
}

void update_context(pt_regs *f, task_struct *process)
{
	// Store the registers.
	process->thread.gs = f->gs;
	process->thread.fs = f->fs;
	process->thread.es = f->es;
	process->thread.ds = f->ds;
	process->thread.edi = f->edi;
	process->thread.esi = f->esi;
	process->thread.ebp = f->ebp;
	process->thread.ebx = f->ebx;
	process->thread.edx = f->edx;
	process->thread.ecx = f->ecx;
	process->thread.eax = f->eax;
	process->thread.eip = f->eip;
	process->thread.eflags = f->eflags;
	process->thread.useresp = f->useresp;
	// TODO: Check if the following registers should be saved.
	// process->thread.cs = f->cs;
	// process->thread.ss = f->ss;
	// Store the FPU.
	switch_fpu();
}

void do_switch(task_struct *process, pt_regs *f)
{
	// Switch to the next process.
	runqueue.curr = process;
	// Restore the registers.
	f->gs = process->thread.gs;
	f->fs = process->thread.fs;
	f->es = process->thread.es;
	f->ds = process->thread.ds;
	f->edi = process->thread.edi;
	f->esi = process->thread.esi;
	f->ebp = process->thread.ebp;
	f->ebx = process->thread.ebx;
	f->edx = process->thread.edx;
	f->ecx = process->thread.ecx;
	f->eax = process->thread.eax;
	f->eip = process->thread.eip;
	f->eflags = process->thread.eflags;
	f->useresp = process->thread.useresp;
	// TODO: Check if the following registers should be restored.
	// f->cs = process->thread.cs;
	// f->ss = process->thread.ss;
	// Restore the FPU.
	unswitch_fpu();
}

int set_user_nice(task_struct *p, long nice)
{
	if (PRIO_TO_NICE(p->se.prio) != nice && nice >= MIN_NICE &&
		nice <= MAX_NICE) {
		p->se.prio = NICE_TO_PRIO(nice);
	}

	return PRIO_TO_NICE(p->se.prio);
}

void enter_user_jmp(uintptr_t location, uintptr_t stack)
{
	// Reset stack pointer for kernel.
	tss_set_stack(0x10, initial_esp);

	// update start execution time.
	runqueue.curr->se.start_runtime = get_millisecond();

	// last context switch time.
	runqueue.curr->se.exec_start = get_millisecond();

	// Jump in location.
	enter_userspace(location, stack);
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
	dbg_print("New nice value would be : %d\n", newNice);

	if (newNice < MIN_NICE) {
		newNice = MIN_NICE;
	}
	if (newNice > MAX_NICE) {
		newNice = MAX_NICE;
	}

	int actualNice = set_user_nice(runqueue.curr, newNice);
	dbg_print("Actual new nice value is: %d\n", actualNice);

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
		errno = ESRCH;

		return (-1);
	}
	if (pid == runqueue.curr->pid) {
		errno = ECHILD;

		return (-1);
	}
	if (options != 0 && options != WNOHANG) {
		errno = EINVAL;

		return (-1);
	}
	if (status == NULL) {
		errno = EFAULT;

		return (-1);
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
		// Save the state.
		(*status) = entry->state; //TODO: da rivedere
		// Remove entry from children of parent.
		list_head_del(&entry->sibling);
		// Delete the task_struct.
		kfree(entry);
		dbg_print("Freeing memory of process %d.\n", ppid);

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

	task_struct *init_proc = kernel_get_running_process(1);
	if (runqueue.curr == init_proc) {
		kernel_panic("Init process cannot call sys_exit!");
	}
	// Set the termination code of the process.
	runqueue.curr->exit_code = (exit_code << 8) & 0xFF00;
	// Set the state of the process to zombie.
	runqueue.curr->state = EXIT_ZOMBIE;
	// If it has children, then init process has to take care of them.
	if (!list_head_empty(&runqueue.curr->children)) {
		dbg_print("Moving children of %s(%d) to init(%d): {\n",
				  runqueue.curr->name, runqueue.curr->pid, init_proc->pid);
		// TODO: Try to plug the list of children instead of iterating.
		list_head *it;
		list_for_each (it, &runqueue.curr->children) {
			task_struct *entry = list_entry(it, task_struct, sibling);
			dbg_print("    [%d] %s\n", entry->pid, entry->name);
			it = entry->sibling.prev;
			list_head_del(&entry->sibling);
			list_head_add_tail(&init_proc->children, &entry->sibling);
			entry->parent = init_proc;
		}
		dbg_print("}\n");
		dbg_print("Listing children of init(%d): {\n", init_proc->pid);
		list_for_each (it, &init_proc->children) {
			task_struct *entry = list_entry(it, task_struct, sibling);
			dbg_print("    [%d] %s\n", entry->pid, entry->name);
		}
		dbg_print("}\n");
	}
	// Free the space occupied by the stack.
	destroy_process_image(runqueue.curr->mm);
	// Debugging message.
	dbg_print("Process %d exited with value %d\n", runqueue.curr->pid,
			  exit_code);
}
