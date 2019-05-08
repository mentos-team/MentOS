///                MentOS, The Mentoring Operating system project
/// @file process.c
/// @brief Process data structures and functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "process.h"
#include "prio.h"
#include "init.h"
#include "panic.h"
#include "kheap.h"
#include "debug.h"
#include "unistd.h"
#include "string.h"
#include "list_head.h"
#include "stdatomic.h"
#include "scheduler.h"

#define PUSH_ON_STACK(stack, type, item)                                       \
	*((type *)(stack -= sizeof(type))) = item

/// @brief The task_struct of the init process.
static task_struct *init_proc;

void exit_handler()
{
	exit(1);
	kernel_panic("I should not be here.\n");
}

task_struct *create_init_process()
{
	dbg_print("Building init process...\n");
	// Create a new task_struct.
	init_proc = kmalloc(sizeof(task_struct));
	// TODO: process is IN USER SPACE! it should be in KERNEL SPACE!
	memset(init_proc, 0, sizeof(task_struct));
	// Set the id of the process.
	init_proc->pid = get_new_pid();
	// Set the name of the process.
	strcpy(init_proc->name, "init");
	// Set the statistics of the process.
	init_proc->se.prio = DEFAULT_PRIO;
	init_proc->se.start_runtime = 0;
	init_proc->se.exec_start = 0;
	init_proc->se.sum_exec_runtime = 0;
	init_proc->se.vruntime = 0;
	// Initialize the list_head.
	list_head_init(&init_proc->run_list);
	// Initialize the children list_head.
	list_head_init(&init_proc->children);
	// Initialize the sibling list_head.
	list_head_init(&init_proc->sibling);
	// Create a new stack segment.
	init_proc->mm = create_process_image(DEFAULT_STACK_SIZE);
	char *stack = (char *)init_proc->mm->start_stack;
	// Clean stack space.
	memset(stack, 0, DEFAULT_STACK_SIZE);
	// Set the base address of the stack.
	char *ebp = (char *)(stack + DEFAULT_STACK_SIZE);
	// Create a pointer to keep track of the top of the stack.
	char *esp = ebp;
	// Set exit_handler as terminating function for init.
	PUSH_ON_STACK(esp, uintptr_t, (uintptr_t)&exit_handler);
	// Set the top address of the stack.
	init_proc->thread.useresp = (uintptr_t)esp;
	// Set the base address of the stack.
	init_proc->thread.ebp = (uintptr_t)ebp;
	// Set the program counter.
	init_proc->thread.eip = (uintptr_t)&main_init;
	// Enable the interrupts.
	init_proc->thread.eflags = init_proc->thread.eflags | EFLAG_IF;
	// Clear the current working directory.
	memset(init_proc->cwd, '\0', MAX_PATH_LENGTH);
	// Set the state of the process as running.
	init_proc->state = TASK_RUNNING;
	// Active the current process.
	enqueue_task(init_proc);

	dbg_print("--------------------------------------------------\n");
	dbg_print("- %s process (PID: %d, eflags: %d)\n", init_proc->name,
			  init_proc->pid, init_proc->thread.eflags);
	dbg_print("\tStack: [0x%p - 0x%p]\n", init_proc->mm->start_stack,
			  init_proc->mm->start_stack + DEFAULT_STACK_SIZE);
	dbg_print("\tebp: 0x%p\n", init_proc->thread.ebp);
	dbg_print("\tesp: 0x%p\n", init_proc->thread.useresp);
	dbg_print("\teip: 0x%p\n", init_proc->thread.eip);
	dbg_print("--------------------------------------------------\n");

	return init_proc;
}

char *get_current_dir_name()
{
	task_struct *current_process = kernel_get_current_process();
	if (current_process != NULL) {
		return strdup(current_process->cwd);
	}

	return kstrdup("/");
}

void sys_getcwd(char *path, size_t size)
{
	task_struct *current_process = kernel_get_current_process();
	if ((current_process != NULL) && (path != NULL)) {
		strncpy(path, current_process->cwd, size);
	}
}

void sys_chdir(char const *path)
{
	task_struct *current_process = kernel_get_current_process();
	if ((current_process != NULL) && (path != NULL)) {
		strcpy(current_process->cwd, path);
	}
}

pid_t sys_vfork(pt_regs *r)
{
	task_struct *current = kernel_get_current_process();
	if (current == NULL) {
		kernel_panic("There is no current process!");
	}

	dbg_print("Forking '%s'(%d) process...\n", current->name, current->pid);

	// Create a new task_struct.
	// TODO: process is IN USER SPACE! it should be in KERNEL SPACE!
	task_struct *new_process = kmalloc(sizeof(task_struct));

	// TODO: this is NOT a deep copy. should a deep copy be used here?
	memcpy(new_process, current, sizeof(task_struct));

	// Set the id of the process.
	new_process->pid = get_new_pid();

	// Set the statistics of the process.
	new_process->se.prio = DEFAULT_PRIO;
	new_process->se.start_runtime = 0;
	new_process->se.exec_start = 0;
	new_process->se.sum_exec_runtime = 0;
	// TODO: vruntime should be the scheduled highest values so far.
	new_process->se.vruntime = current->se.vruntime;

	// Create a new stack segment.
	new_process->mm = create_process_image(DEFAULT_STACK_SIZE);
	char *stack = (char *)new_process->mm->start_stack;
	// Copy the father's stack.
	memcpy((char *)new_process->mm->start_stack,
		   (char *)current->mm->start_stack, DEFAULT_STACK_SIZE);
	// Set the base address of the stack.
	char *ebp = stack + DEFAULT_STACK_SIZE; // TODO: da controllare
	// Create a pointer to keep track of the top of the stack.
	char *esp = stack + (r->useresp - current->mm->start_stack);

	// Set the top address of the stack.
	new_process->thread.useresp = (uintptr_t)esp;
	// Set the base address of the stack.
	new_process->thread.ebp = (uintptr_t)ebp;
	// Set the program counter.
	new_process->thread.eip = r->eip;

	// Set the base registers.
	new_process->thread.eax = 0;
	new_process->thread.ebx = r->ebx;
	new_process->thread.ecx = r->ecx;
	new_process->thread.edx = r->edx;

	// Enable the interrupts.
	new_process->thread.eflags = new_process->thread.eflags | EFLAG_IF;

	// Set the state of the process as running.
	new_process->state = TASK_RUNNING;

	// Set current as parent for the new process
	new_process->parent = current;

	// Initialize the list_head.
	list_head_init(&new_process->run_list);

	// Initialize the children list_head.
	list_head_init(&new_process->children);

	// Initialize the children list_head.
	list_head_init(&new_process->sibling);

	// Set the new_process as child of current.
	list_head_add_tail(&current->children, &new_process->sibling);

	// Active the new process.
	enqueue_task(new_process);

	dbg_print("--------------------------------------------------\n");
	dbg_print("- %s process (PID: %d, eflags: %d)\n", new_process->name,
			  new_process->pid, new_process->thread.eflags);
	dbg_print("\teip    : 0x%p\n", new_process->thread.eip);
	dbg_print("\tebp    : 0x%p\n", new_process->thread.ebp);
	dbg_print("\tesp    : 0x%p\n", new_process->thread.useresp);
	dbg_print("\tStack  : 0x%p\n", new_process->mm->start_stack);
	dbg_print("\tRunList: 0x%p\n", &new_process->run_list);
	dbg_print("--------------------------------------------------\n");

	dbg_print("Fork of '%s' (child pid: %d) process completed.\n",
			  current->name, current->pid);

	// Return PID of child process to parent.
	return new_process->pid;
}

static inline int push_args_on_stack(uintptr_t *esp, char *args[],
									 char ***argsptr)
{
	int argc = 0;
	char *args_ptr[256];
	// Count the number of arguments.
	while (args[argc] != NULL) {
		++argc;
	}
	// Push terminating NULL.
	PUSH_ON_STACK((*esp), char *, (char *)NULL);
	// Prepare args with space for the terminating NULL.
	for (int i = argc - 1; i >= 0; --i) {
		for (int j = strlen(args[i]); j >= 0; --j) {
			PUSH_ON_STACK((*esp), char, args[i][j]);
		}
		args_ptr[i] = (char *)(*esp);
	}
	// Push terminating NULL.
	PUSH_ON_STACK((*esp), char *, (char *)NULL);
	// Push array of pointers to the arguments.
	for (int i = argc - 1; i >= 0; --i) {
		PUSH_ON_STACK((*esp), char *, args_ptr[i]);
	}
	(*argsptr) = (char **)(*esp);

	return argc;
}

int sys_execve(pt_regs *r)
{
	char **argv, **_argv, **envp, **_envp;
	// Check the current process.
	task_struct *current = kernel_get_current_process();
	if (current == NULL) {
		kernel_panic("There is no current process!");
	}

	// Get the filename.
	uintptr_t *filename = (uintptr_t *)r->ebx;
	if (filename == NULL) {
		return -1;
	}

	// Get the arguments.
	argv = (char **)r->ecx;
	// Get the environment.
	envp = (char **)r->edx;

	// Check the argument and that at least the name is provided.
	if ((argv == NULL) || (argv[0] == NULL)) {
		return -1;
	}

	// Check that the environment is provided.
	if (envp == NULL) {
		kernel_panic("You must provide at least an empty list for envp!");
	}

	// Set the name.
	strcpy(current->name, argv[0]);

	// Set the top address of the stack.
	current->thread.useresp = (uintptr_t)current->thread.ebp;

	// Set the program counter.
	current->thread.eip = (uintptr_t)filename;

	int argc = push_args_on_stack(&current->thread.useresp, argv, &_argv);
	push_args_on_stack(&current->thread.useresp, envp, &_envp);

	PUSH_ON_STACK(current->thread.useresp, char **, _envp);
	PUSH_ON_STACK(current->thread.useresp, char **, _argv);
	PUSH_ON_STACK(current->thread.useresp, int, argc);
	PUSH_ON_STACK(current->thread.useresp, uintptr_t, (uintptr_t)exit_handler);

//	dbg_print("_ARGV:0x%09x {\n", _argv);
//	for (int i = 0; _argv[i] != NULL; ++i) {
//		dbg_print("\t[%d][0x%09x]%s\n", i, _argv[i], _argv[i]);
//	}
//	dbg_print("}\n");
//
//	if (_envp != NULL) {
//		dbg_print("_ENVP:0x%09x {\n", _envp);
//		for (int i = 0; _envp[i] != NULL; ++i) {
//			dbg_print("\t[%d][0x%09x]%s\n", i, _envp[i], _envp[i]);
//		}
//		dbg_print("}\n");
//	}

	// Perform the switch to the new process.
	do_switch(current, r);

	dbg_print("Executing '0x%p' for process %d with %d arguments (0x%p)...\n",
			  filename, current->pid, argc, argv);

	return 0;
}
