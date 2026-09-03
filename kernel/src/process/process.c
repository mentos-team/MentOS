/// @file process.c
/// @brief Process data structures and functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PROC  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "assert.h"
#include "elf/elf.h"
#include "errno.h"
#include "fcntl.h"
#include "fs/namei.h"
#include "fs/vfs.h"
#include "hardware/timer.h"
#include "klib/stack_helper.h"
#include "libgen.h"
#include "mem/mm/mm.h"
#include "mem/mm/vmem.h"
#include "process/pid_manager.h"
#include "process/prio.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "process/wait.h"
#include "string.h"
#include "sys/stat.h"
#include "system/panic.h"
#include "unistd.h"

/// Cache for creating the task structs.
static kmem_cache_t *task_struct_cache;

/// @brief Counts the number of arguments.
/// @param args the array of arguments, it must be NULL terminated.
/// @param max_count the maximum number of entries to scan.
/// @return the number of arguments, or -E2BIG when the vector is not
///         NULL-terminated within `max_count` entries.
static inline int __count_args(char **args, int max_count)
{
    int argc = 0;
    while ((argc < max_count) && (args[argc] != NULL)) {
        ++argc;
    }
    if ((argc == max_count) && (args[argc] != NULL)) {
        return -E2BIG;
    }
    return argc;
}

/// @brief Counts the bytes occupied by the arguments.
/// @param args the array of arguments, it must be NULL terminated.
/// @param argc the number of arguments, already validated by __count_args.
/// @param out_bytes where the total is stored: the string bytes (each
///        including its terminator) plus the pointer array.
/// @return 0 on success, -E2BIG when a string is not NUL-terminated within
///         MAX_ARG_STRLEN bytes, or the total exceeds ARG_MAX.
static inline int __count_args_bytes(char **args, int argc, int *out_bytes)
{
    // Count the characters, bounding each string: a non-terminated string
    // must not turn the walk into an unbounded kernel read (#196).
    int nchar = 0;
    for (int i = 0; i < argc; i++) {
        size_t len = strnlen(args[i], MAX_ARG_STRLEN);
        if (len >= MAX_ARG_STRLEN) {
            return -E2BIG;
        }
        nchar += (int)len + 1;
        if (nchar > ARG_MAX) {
            return -E2BIG;
        }
    }
    *out_bytes = nchar + ((argc + 1 /* The NULL terminator */) * (int)sizeof(char *));
    if (*out_bytes > ARG_MAX) {
        return -E2BIG;
    }
    return 0;
}

/// @brief Pushes the argument strings on the stack (growing downwards),
/// recording the final position of each string.
/// @param stack pointer to the stack location.
/// @param args the list of arguments; the strings must be kernel copies,
///        their lengths are trusted because they were validated on copy.
/// @param argc the number of arguments, already validated by the caller.
/// @param locations array of at least `argc` entries where the position of
///        each string is stored; the caller owns it, sized from the
///        validated count (it replaces the fixed `char *[256]` that
///        overflowed the kernel stack for larger vectors, #196).
static inline void __push_strings_on_stack(uintptr_t *stack, char *args[], int argc, char *locations[])
{
    for (int i = argc - 1; i >= 0; --i) {
        for (int j = strlen(args[i]); j >= 0; --j) {
            stack_push_u8((uint32_t *)stack, args[i][j]);
        }
        locations[i] = (char *)(*stack);
    }
}

/// @brief Pushes the strings of a user-controlled vector on the stack, with
///        a per-string bound and a total-budget floor.
/// @param stack pointer to the stack location.
/// @param args the list of arguments, straight from user memory.
/// @param argc the number of arguments, already validated by __count_args.
/// @param locations array of at least `argc` entries, caller-owned.
/// @param floor the lowest address the pushes may reach: the strings were
///        counted before, and a string that grew since then must fail with
///        -E2BIG here rather than push more bytes than were accounted for
///        (which would write below the allocation).
/// @return 0 on success, -E2BIG when a string is not NUL-terminated within
///         MAX_ARG_STRLEN bytes, or the pushes would cross `floor`.
static inline int
__push_user_strings_on_stack(uintptr_t *stack, char *args[], int argc, char *locations[], uintptr_t floor)
{
    for (int i = argc - 1; i >= 0; --i) {
        size_t len = strnlen(args[i], MAX_ARG_STRLEN);
        if (len >= MAX_ARG_STRLEN) {
            return -E2BIG;
        }
        if ((*stack - (len + 1)) < floor) {
            return -E2BIG;
        }
        for (int j = (int)len; j >= 0; --j) {
            stack_push_u8((uint32_t *)stack, args[i][j]);
        }
        locations[i] = (char *)(*stack);
    }
    return 0;
}

/// @brief Pushes the terminating NULL and the array of string pointers.
/// @param stack pointer to the stack location.
/// @param locations the positions of the strings, filled by the string push.
/// @param argc the number of arguments.
/// @return the final position of the stack, where the pointer array is stored.
static inline char **__push_vector_on_stack(uintptr_t *stack, char *locations[], int argc)
{
    // Push terminating NULL.
    stack_push_ptr((uint32_t *)stack, NULL);
    // Push array of pointers to the arguments.
    for (int i = argc - 1; i >= 0; --i) {
        stack_push_ptr((uint32_t *)stack, locations[i]);
    }
    return (char **)(*stack);
}

/// @brief Pushes the arguments on the stack.
/// @param stack pointer to the stack location.
/// @param args the list of arguments; the strings must be kernel copies.
/// @param argc the number of arguments, already validated by the caller.
/// @param locations array of at least `argc` entries, caller-owned.
/// @return the final position of the stack, where the list of pushed arguments is stored.
static inline char **__push_args_on_stack(uintptr_t *stack, char *args[], int argc, char *locations[])
{
    __push_strings_on_stack(stack, args, argc, locations);
    return __push_vector_on_stack(stack, locations, argc);
}

/// @brief Clears the user stack of a freshly created memory descriptor.
/// @param mm the memory descriptor whose stack must be cleared.
/// @return 0 on success, -errno on failure.
static int __clear_user_stack(mm_struct_t *mm)
{
    // Map the stack of the candidate image inside the kernel virtual mapping
    // window, so that we can clear it without switching the current page
    // directory (see #208: the old image must stay active while the new one
    // is being built).
    virt_map_page_t *vpage = vmem_map_alloc_virtual(DEFAULT_STACK_SIZE);
    if (vpage == NULL) {
        pr_err("Failed to allocate the virtual mapping window for the stack.\n");
        return -ENOMEM;
    }
    uint32_t dst_addr = vmem_map_virtual_address(mm, vpage, mm->start_stack, DEFAULT_STACK_SIZE);
    if (dst_addr == 0) {
        pr_err("Failed to map the virtual mapping window for the stack.\n");
        vmem_unmap_virtual_address_page(vpage);
        return -ENOMEM;
    }
    // Clean stack space.
    memset((char *)dst_addr, 0, DEFAULT_STACK_SIZE);
    // Release the kernel virtual mapping window.
    vmem_unmap_virtual_address_page(vpage);
    return 0;
}

/// @brief Checks if the file starts with a shebang.
/// @param file the file to check.
/// @return 1 if it contains a shebang, 0 otherwise.
static int __has_shebang(vfs_file_t *file)
{
    char buf[2];
    // Only classify the file as a script when the whole marker was read,
    // otherwise the buffer content would be uninitialized.
    if (vfs_read(file, buf, 0, sizeof(buf)) != (ssize_t)sizeof(buf)) {
        return 0;
    }
    return buf[0] == '#' && buf[1] == '!';
}

/// @brief Builds the memory image of an executable into a candidate mm.
/// @param path the path of the executable to load.
/// @param task the task requesting the executable (used for permissions and
///             for the setuid/setgid handling); it is left untouched until
///             the image is fully built.
/// @param entry the output where the entry point is stored (only written on
///              success).
/// @param new_mm the output where the candidate memory descriptor is stored
///               (only written on success). The caller owns it and must
///               either install it on the task (commit) or destroy it
///               (rollback).
/// @return -errno on failure, 1 on success, 2 if an interpreter was loaded.
static int __load_executable(const char *path, task_struct *task, uint32_t *entry, mm_struct_t **new_mm)
{
    // Return code variable.
    int ret                = 0;
    int interpreter_loop   = 0;
    // The duplicated interpreter path, it must be freed on every exit path.
    char *interpreter_path = NULL;
    // The candidate memory image: until the load succeeds it is completely
    // separate from the running image of the task (#208).
    mm_struct_t *candidate = NULL;
    // The credentials are restored if the load fails: a failed execve must
    // leave the caller exactly as it was.
    uid_t saved_uid        = task->uid;
    gid_t saved_gid        = task->gid;
start:
    pr_debug("__load_executable(`%s`, %p `%s`, %p)\n", path, task, task->name, entry);
    vfs_file_t *file = vfs_open(path, O_RDONLY, 0);
    if (file == NULL) {
        pr_err("Cannot find executable!\n");
        ret = -errno;
        goto close_and_return;
    }
    if (!file->fs_operations || !file->sys_operations) {
        pr_err("Executable has no filesystem operations (unmounted fs?).\n");
        ret = -ENOENT;
        goto close_and_return;
    }
    // Check that the file has the execute permission set
    if (!vfs_valid_exec_permission(task, file)) {
        pr_err("This is not executable `%s`!\n", path);
        ret = -EACCES;
        goto close_and_return;
    }
    // Check that the file is actually an executable before building the
    // candidate image.
    if (!(elf_check_file_type(file, ET_EXEC) || __has_shebang(file))) {
        pr_debug("This is not a valid executable `%s`!\n", path);
        ret = -ENOEXEC;
        goto close_and_return;
    }
    // Set the effective uid if the setuid bit is present.
    if (bitmask_check(file->mask, S_ISUID)) {
        task->uid = file->uid;
    }
    // Set the effective gid if the setgid bit is present.
    if (bitmask_check(file->mask, S_ISGID)) {
        task->gid = file->gid;
    }
    // Load potential interpreter specified by a shebang line. The
    // interpreter is resolved before anything is allocated, so that a
    // failing script only costs a file lookup.
    if (__has_shebang(file)) {
        // Disallow interpreter loops
        if (interpreter_loop) {
            ret = -ELOOP;
            goto close_and_return;
        }

        // Read the shebang line, keep one byte free for the terminator.
        char buf[PATH_MAX];
        ssize_t bytes_read = vfs_read(file, buf, 2, sizeof(buf) - 1);
        // The reference to the script file is no longer needed.
        vfs_close(file);
        file = NULL;
        // Never use a failed read result as a buffer index. Do not log
        // `path` here: on the first iteration it is a raw user pointer.
        if (bytes_read < 0) {
            pr_err("Failed to read the shebang line.\n");
            ret = -EIO;
            goto close_and_return;
        }
        buf[bytes_read] = 0;

        // Find end of the line
        char *lineend = strchr(buf, '\n');
        if (!lineend) {
            ret = -ENAMETOOLONG;
            goto close_and_return;
        }
        *lineend = 0;

        interpreter_path = strdup(buf);
        if (interpreter_path == NULL) {
            ret = -ENOMEM;
            goto close_and_return;
        }
        path = interpreter_path;
        interpreter_loop++;
        goto start;
    }

    // == Build the candidate image ===========================================
    // From this point on, every failure must destroy the candidate image
    // and leave the current image of the task untouched.
    // FIXME: When threads will be implemented
    // they should share the mm, so the destroy_process_image must be called
    // only when all the threads are terminated. This can be accomplished by using
    // an internal counter on the mm.
    candidate = mm_create_blank(DEFAULT_STACK_SIZE);
    if (candidate == NULL) {
        pr_err("Failed to initialize the candidate mm structure.\n");
        ret = -ENOMEM;
        goto close_and_return;
    }
    // Clear the stack of the candidate image through the kernel virtual
    // mapping window: the current page directory stays on the old image.
    if ((ret = __clear_user_stack(candidate)) < 0) {
        goto close_and_return;
    }

    // Load the elf file into the candidate memory descriptor.
    if ((ret = elf_load_file(candidate, file, entry)) < 0) {
        pr_err("Failed to load ELF file `%s`!\n", path);
        goto close_and_return;
    }
    ret = interpreter_loop ? 2 : 1;

close_and_return:
    // Close the file if it is still open.
    if (file != NULL) {
        vfs_close(file);
    }
    // Rollback: destroy the candidate image and restore the credentials. On
    // success, hand the candidate over to the caller, which becomes
    // responsible for committing (or destroying) it.
    if (ret <= 0) {
        if (candidate != NULL) {
            mm_destroy(candidate);
            candidate = NULL;
        }
        task->uid = saved_uid;
        task->gid = saved_gid;
    } else {
        *new_mm = candidate;
    }
    // Free the duplicated interpreter path.
    if (interpreter_path != NULL) {
        kfree(interpreter_path);
    }
    return ret;
}

/// @brief Allocates the memory for a task.
/// @param source the source task we use for the copy.
/// @param parent the parent process.
/// @param name the name of the new process.
/// @return pointer to the newly allocated task.
static inline task_struct *__alloc_task(task_struct *source, task_struct *parent, const char *name)
{
    // Create a new task_struct.
    task_struct *proc = kmem_cache_alloc(task_struct_cache, GFP_KERNEL);
    // Clear the memory.
    memset(proc, 0, sizeof(task_struct));
    // Set the id of the process.
    proc->pid   = pid_manager_get_free_pid();
    // Set the state of the process as running.
    proc->state = TASK_RUNNING;
    // Set the current opened file descriptors and the maximum number of file descriptors.
    if (source) {
        vfs_dup_task(proc, source);
    } else {
        vfs_init_task(proc);
    }
    // Set the pointer to process's parent.
    proc->parent = parent;
    // Initialize the list_head.
    list_head_init(&proc->run_list);
    // Initialize the children list_head.
    list_head_init(&proc->children);
    // Initialize the sibling list_head.
    list_head_init(&proc->sibling);
    // If we have a parent, set the sibling child relation.
    if (parent) {
        // Set the new_process as child of current.
        list_head_insert_before(&proc->sibling, &parent->children);
    }
    if (source) {
        memcpy(&proc->thread, &source->thread, sizeof(thread_struct_t));
    }
    // Set the statistics of the process.
    proc->uid                   = 0;
    proc->ruid                  = 0;
    proc->gid                   = 0;
    proc->rgid                  = 0;
    proc->sid                   = 0;
    proc->pgid                  = 0;
    proc->se.prio               = DEFAULT_PRIO;
    proc->se.start_runtime      = timer_get_ticks();
    proc->se.exec_start         = timer_get_ticks();
    proc->se.exec_runtime       = 0;
    proc->se.sum_exec_runtime   = 0;
    proc->se.vruntime           = 0;
    proc->se.period             = 0;
    proc->se.deadline           = 0;
    proc->se.arrivaltime        = timer_get_ticks();
    proc->se.executed           = false;
    proc->se.is_periodic        = false;
    proc->se.is_under_analysis  = false;
    proc->se.next_period        = 0;
    proc->se.worst_case_exec    = 0;
    proc->se.utilization_factor = 0;
    // Initialize the exit code of the process.
    proc->exit_code             = 0;
    // Copy the name, bounded to the field: sources are kernel literals and
    // parent names already bounded by sys_execve, this keeps the copy safe
    // even if a future caller grows the name.
    if (name) {
        strncpy(proc->name, name, sizeof(proc->name) - 1);
        proc->name[sizeof(proc->name) - 1] = '\0';
    }
    // Do not touch the task's segments.
    proc->mm       = NULL;
    // Initialize the error number.
    proc->error_no = 0;
    // Initialize the current working directory.
    if (source) {
        strcpy(proc->cwd, source->cwd);
    } else {
        strcpy(proc->cwd, "/");
    }
    // Clear the signal handler.
    memset(&proc->sighand, 0x00, sizeof(sighand_t));
    spinlock_init(&proc->sighand.siglock);
    atomic_set(&proc->sighand.count, 0);
    for (int i = 0; i < NSIG; ++i) {
        proc->sighand.action[i].sa_handler = SIG_DFL;
        sigemptyset(&proc->sighand.action[i].sa_mask);
        proc->sighand.action[i].sa_flags = 0;
    }
    // Clear the masks.
    sigemptyset(&proc->blocked);
    sigemptyset(&proc->real_blocked);
    sigemptyset(&proc->saved_sigmask);
    // Initialzie the data structure storing the pending signals.
    list_head_init(&proc->pending.list);
    sigemptyset(&proc->pending.signal);

    // Initalize real_timer for intervals
    proc->real_timer = NULL;

    // Set the default terminal options.
    proc->termios = (termios_t){
        .c_cflag = 0,
        .c_lflag = (ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG),
        .c_oflag = 0,
        .c_iflag = 0,
    };
    // Initialize the ringbuffer.
    rb_keybuffer_init(&proc->keyboard_rb);

    return proc;
}

int init_tasking(void)
{
    if ((task_struct_cache = KMEM_CREATE(task_struct)) == NULL) {
        return 0;
    }
    return 1;
}

int process_create_init(const char *path)
{
    pr_debug("Building init process...\n");

    // Allocate the memory for the process.
    init_process = __alloc_task(NULL, NULL, "init");

    // Active the current process.
    scheduler_enqueue_task(init_process);

    // == INITIALIZE `/proc/video` ============================================
    // Check that the fd_list is initialized.
    assert(init_process->fd_list && "File descriptor list not initialized.");
    assert((init_process->max_fd > 3) && "File descriptor list cannot contain the standard IOs.");

    // Create STDIN descriptor.
    vfs_file_t *vfs_stdin = vfs_open("/proc/video", O_RDONLY, 0);
    vfs_stdin->count++;
    init_process->fd_list[STDIN_FILENO].file_struct = vfs_stdin;
    init_process->fd_list[STDIN_FILENO].flags_mask  = O_RDONLY;
    pr_debug("`/proc/video` stdin  : %p\n", vfs_stdin);

    // Create STDOUT descriptor.
    vfs_file_t *vfs_stdout = vfs_open("/proc/video", O_WRONLY, 0);
    vfs_stdout->count++;
    init_process->fd_list[STDOUT_FILENO].file_struct = vfs_stdout;
    init_process->fd_list[STDOUT_FILENO].flags_mask  = O_WRONLY;
    pr_debug("`/proc/video` stdout : %p\n", vfs_stdout);

    // Create STDERR descriptor.
    vfs_file_t *vfs_stderr = vfs_open("/proc/video", O_WRONLY, 0);
    vfs_stderr->count++;
    init_process->fd_list[STDERR_FILENO].file_struct = vfs_stderr;
    init_process->fd_list[STDERR_FILENO].flags_mask  = O_WRONLY;
    pr_debug("`/proc/video` stderr : %p\n", vfs_stderr);
    // ------------------------------------------------------------------------

    // == INITIALIZE TASK MEMORY ==============================================
    // Build the image of the executable into a candidate mm; the task is
    // not touched until the image is fully built (#208).
    mm_struct_t *new_mm = NULL;
    if (__load_executable(path, init_process, &init_process->thread.regs.eip, &new_mm) <= 0) {
        pr_err("Failed to load the init executable: %s.\n", path);
        return 1;
    }
    // ------------------------------------------------------------------------

    // == INITIALIZE PROGRAM ARGUMENTS ========================================
    // Save the current page directory.
    page_directory_t *crtdir = paging_get_current_pgd();
    // Switch to the page directory of the candidate image.
    paging_switch_pgd(new_mm->pgd);

    // Commit: the candidate image becomes the image of the init process
    // (there is no previous image to destroy).
    init_process->mm        = new_mm;
    // The stack of the new image starts at its top.
    uintptr_t useresp       = new_mm->start_stack + DEFAULT_STACK_SIZE;

    // Prepare argv and envp for the init process.
    char **argv_ptr;
    char **envp_ptr;
    int argc                    = 1;
    static char *argv[]         = {"/bin/init", (char *)NULL};
    static char *envp[]         = {(char *)NULL};
    // The positions of the pushed strings, for the pointer arrays: the
    // vectors are kernel literals with one entry, so a small stack array
    // is enough here (sys_execve sizes it from the validated count).
    char *argv_locations[4];
    char *envp_locations[4];
    // Save where the arguments start.
    new_mm->arg_start           = useresp;
    // Push the arguments on the stack.
    argv_ptr                    = __push_args_on_stack(&useresp, argv, 1, argv_locations);
    // Save where the arguments end.
    new_mm->arg_end             = useresp;
    // Save where the environmental variables start.
    new_mm->env_start           = useresp;
    // Push the environment on the stack.
    envp_ptr                    = __push_args_on_stack(&useresp, envp, 0, envp_locations);
    // Save where the environmental variables end.
    new_mm->env_end             = useresp;
    // Push the `main` arguments on the stack (argc, argv, envp).
    stack_push_ptr(&useresp, envp_ptr);
    stack_push_ptr(&useresp, argv_ptr);
    stack_push_s32(&useresp, argc);

    // Set the stack registers of the new image, and enable the interrupts.
    init_process->thread.regs.ebp     = useresp;
    init_process->thread.regs.useresp = useresp;
    init_process->thread.regs.eflags  = init_process->thread.regs.eflags | EFLAG_IF;

    // Restore previous pgdir
    paging_switch_pgd(crtdir);
    // ------------------------------------------------------------------------

    pr_debug("Executing '%s' (pid: %d)...\n", init_process->name, init_process->pid);

    return 0;
}

vfs_file_descriptor_t *fget(int fd)
{
    task_struct *current = scheduler_get_current_process();
    assert(current && "There is no current task running.");
    // Check the current FD.
    if (fd < 0 || fd >= current->max_fd) {
        return NULL;
    }
    // Retrieve the file structure from the table.
    return current->fd_list + fd;
}

char *sys_getcwd(char *buf, size_t size)
{
    task_struct *current = scheduler_get_current_process();
    if ((current == NULL) || (buf == NULL)) {
        return (char *)-EACCES;
    }
    if (size == 0) {
        return (char *)-EINVAL;
    }
    size_t len = strnlen(current->cwd, sizeof(current->cwd));
    if (size < (len + 1)) {
        return (char *)-ERANGE;
    }
    memcpy(buf, current->cwd, len + 1);
    return buf;
}

int sys_chdir(char const *path)
{
    task_struct *current = scheduler_get_current_process();
    assert(current && "There is no running process.");
    if (!path) {
        return -EFAULT;
    }
    char absolute_path[PATH_MAX];
    if (resolve_path(path, absolute_path, sizeof(absolute_path), REMOVE_TRAILING_SLASH | FOLLOW_LINKS) < 0) {
        pr_err("Cannot get the absolute path for path `%s`.\n", path);
        return -errno;
    }
    // Check that the directory exists.
    vfs_file_t *dir = vfs_open(absolute_path, O_RDONLY | O_DIRECTORY, S_IXUSR);
    if (dir) {
        strcpy(current->cwd, absolute_path);
        vfs_close(dir);
        return 0;
    }
    // Return the errno value set by either VFS or the filesystem underneath.
    return -errno;
}

int sys_fchdir(int fd)
{
    task_struct *current = scheduler_get_current_process();
    assert(current && "There is no running process.");
    // Check if it is a valid file descriptor.
    if ((fd < 0) || (fd >= current->max_fd)) {
        return -EBADF;
    }
    // Get the file descriptor.
    vfs_file_descriptor_t *vfd = &current->fd_list[fd];
    // Check if the file descriptor file is set.
    if (vfd->file_struct == NULL) {
        return -ENOENT;
    }
    // Check that the path points to a directory.
    if (!bitmask_check(vfd->file_struct->flags, DT_DIR)) {
        return -ENOTDIR;
    }
    char absolute_path[PATH_MAX];
    if (resolve_path(
            vfd->file_struct->name, absolute_path, sizeof(absolute_path), REMOVE_TRAILING_SLASH | FOLLOW_LINKS) < 0) {
        pr_err("Cannot get the absolute path for path `%s`.\n", vfd->file_struct->name);
        return -ENOENT;
    }
    strcpy(current->cwd, absolute_path);
    return 0;
}

pid_t sys_fork(pt_regs_t *f)
{
    task_struct *current = scheduler_get_current_process();
    if (current == NULL) {
        kernel_panic("There is no current process!");
    }

    pr_debug("Forking   '%s' (pid: %d)...\n", current->name, current->pid);

    // Update current process registers, they should be equal
    // to the ones of the child process, except for eax.
    scheduler_store_context(f, current);
    // Allocate the memory for the process.
    task_struct *proc        = __alloc_task(current, current, current->name);
    // Copy the father's stack, memory, heap etc... to the child process
    proc->mm                 = mm_clone(current->mm);
    // Set the eax as 0, to indicate the child process
    proc->thread.regs.eax    = 0;
    // Enable the interrupts.
    proc->thread.regs.eflags = proc->thread.regs.eflags | EFLAG_IF;

    // Copy session and group id of the parent into the child
    proc->sid  = current->sid;
    proc->pgid = current->pgid;
    proc->uid  = current->uid;
    proc->ruid = current->ruid;
    proc->gid  = current->gid;
    proc->rgid = current->rgid;

    // Active the new process.
    scheduler_enqueue_task(proc);

    pr_debug(
        "Forked    '%s' (pid: %d, gid: %d, sid: %d, pgid: %d)...\n", proc->name, proc->pid, proc->gid, proc->sid,
        proc->pgid);

    // Return PID of child process to parent.
    return proc->pid;
}

int sys_execve(pt_regs_t *f)
{
    // Check the current process.
    task_struct *current = scheduler_get_current_process();
    if (current == NULL) {
        kernel_panic("There is no current process!");
    }

    char **origin_argv;
    char **saved_argv;
    char **final_argv;
    char **origin_envp;
    char **saved_envp;
    char **final_envp;
    char name_buffer[NAME_MAX];
    char saved_filename[PATH_MAX];

    // Get the filename.
    char *filename = (char *)f->ebx;
    if (filename == NULL) {
        pr_err("Received NULL filename.\n");
        return -EFAULT;
    }
    // Get the arguments
    origin_argv = (char **)f->ecx;
    // Get the environment.
    origin_envp = (char **)f->edx;
    // Check the argument, the environment, and that at least the name is provided.
    if (origin_argv == NULL) {
        pr_err("sys_execve failed: must provide argv.\n");
        return -EFAULT;
    }
    if (origin_argv[0] == NULL) {
        pr_err("sys_execve failed: must provide the name.\n");
        return -EINVAL;
    }
    if (origin_envp == NULL) {
        // We allow a NULL environment, using a default, for macOS compatibility
        pr_debug("sys_execve: NULL envp, using default environment.\n");
        static char *default_env[] = {
            "PATH=/bin:/usr/bin",
            "HOME=/",
            NULL
        };
        origin_envp = default_env;
    }

    // A filename that does not fit a PATH_MAX buffer cannot name any file,
    // and truncating it would target the wrong executable: reject it instead
    // of copying it. The strnlen walk is bounded to PATH_MAX.
    if (strnlen(filename, PATH_MAX) >= PATH_MAX) {
        pr_err("sys_execve failed: filename is longer than PATH_MAX.\n");
        return -ENAMETOOLONG;
    }
    // Save the name of the process. argv[0] is a raw user string: copy at
    // most what name_buffer can hold minus its terminator, truncating like
    // Linux truncates comm, so a long argv[0] neither fails the exec nor
    // overflows kernel state.
    size_t name_len = strnlen(origin_argv[0], sizeof(name_buffer) - 1);
    memcpy(name_buffer, origin_argv[0], name_len);
    name_buffer[name_len] = '\0';
    // Save the filename: the check above bounds it to PATH_MAX - 1
    // characters, so it always fits with its terminator.
    strcpy(saved_filename, filename);

    // == COPY PROGRAM ARGUMENTS ==============================================
    // Copy argv and envp to kernel memory, because all the old process memory will be discarded.
    // Every count is bounded: a vector that is not NULL-terminated within
    // MAX_ARG_COUNT entries, a string without a terminator within
    // MAX_ARG_STRLEN bytes, or an argv/envp above ARG_MAX fails with
    // -E2BIG, instead of walking user memory unbounded and overflowing
    // kernel state (#196).
    int argc;
    int envc;
    int argv_bytes;
    int envp_bytes;
    if (((argc = __count_args(origin_argv, MAX_ARG_COUNT)) < 0) ||
        ((envc = __count_args(origin_envp, MAX_ARG_COUNT)) < 0)) {
        pr_err("sys_execve failed: too many arguments or environment entries.\n");
        return -E2BIG;
    }
    if ((__count_args_bytes(origin_argv, argc, &argv_bytes) < 0) ||
        (__count_args_bytes(origin_envp, envc, &envp_bytes) < 0)) {
        pr_err("sys_execve failed: arguments or environment exceed ARG_MAX.\n");
        return -E2BIG;
    }
    void *args_mem = kmalloc(argv_bytes + envp_bytes);
    if (!args_mem) {
        pr_err(
            "Failed to allocate memory for arguments and environment %d (%d + "
            "%d).\n",
            argv_bytes + envp_bytes, argv_bytes, envp_bytes);
        return -ENOMEM;
    }
    // The arrays of string positions are sized from the validated counts:
    // the argv one also covers the interpreter path, which shifts argv by
    // two entries and therefore needs argc + 2 slots.
    char **argv_locations = kmalloc((argc + 2) * sizeof(char *));
    char **envp_locations = kmalloc(((envc > 0) ? envc : 1) * sizeof(char *));
    if (!argv_locations || !envp_locations) {
        pr_err("Failed to allocate memory for the argument positions.\n");
        kfree(argv_locations);
        kfree(envp_locations);
        kfree(args_mem);
        return -ENOMEM;
    }
    // Copy the arguments (raw user strings, bounded per string and against
    // the total budget: one that grew after the counting fails here instead
    // of pushing more bytes than were accounted for). The argv pushes must
    // stay above the environment region of the block.
    uint32_t args_mem_ptr = (uint32_t)args_mem + (argv_bytes + envp_bytes);
    if (__push_user_strings_on_stack(&args_mem_ptr, origin_argv, argc, argv_locations, (uint32_t)args_mem + envp_bytes) < 0) {
        pr_err("sys_execve failed: an argument is not terminated within the limit.\n");
        kfree(argv_locations);
        kfree(envp_locations);
        kfree(args_mem);
        return -E2BIG;
    }
    saved_argv = __push_vector_on_stack(&args_mem_ptr, argv_locations, argc);
    if (__push_user_strings_on_stack(&args_mem_ptr, origin_envp, envc, envp_locations, (uint32_t)args_mem) < 0) {
        pr_err("sys_execve failed: an environment entry is not terminated within the limit.\n");
        kfree(argv_locations);
        kfree(envp_locations);
        kfree(args_mem);
        return -E2BIG;
    }
    saved_envp = __push_vector_on_stack(&args_mem_ptr, envp_locations, envc);
    // Check the memory pointer.
    assert(args_mem_ptr == (uint32_t)args_mem);
    // ------------------------------------------------------------------------

    // == INITIALIZE TASK MEMORY ==============================================
    // Build the new image inside a candidate mm: until the commit below,
    // the current image of the process stays fully functional, so any
    // failure just returns an error to the caller (#208).
    uint32_t entry      = 0;
    mm_struct_t *new_mm = NULL;
    // Credentials are restored if any of the post-load steps fails.
    uid_t prev_uid      = current->uid;
    gid_t prev_gid      = current->gid;
    int ret             = __load_executable(filename, current, &entry, &new_mm);
    if (ret <= 0) {
        pr_err("Failed to load executable!\n");
        // Free the temporary args memory.
        kfree(args_mem);
        return ret;
    }
    if (ret == 2) { // An interpreter was loaded.
        // We need to modify the argv array passed to the interpreter process.
        // The original file name must be passed as second argument and the rest
        // is shifted to the right.
        // Prepare a new argv array.
        char **int_argv = kmalloc((argc + 2) * sizeof(char *));
        if (!int_argv) {
            pr_err("Failed to allocate memory for interpreter argv array.\n");
            // Rollback: the old image is still the current one.
            mm_destroy(new_mm);
            current->uid = prev_uid;
            current->gid = prev_gid;
            kfree(args_mem);
            return -ENOMEM;
        }
        int_argv[0] = saved_argv[0]; // TODO: pass the path to the interpreter.
        int_argv[1] = saved_filename;
        for (int i = 1; i <= argc; i++) {
            int_argv[i + 1] = saved_argv[i];
        }
        argc++;

        // Rebuild the saved argv and envp pointers. The buffer must hold both
        // the new argv and the whole environment (#227).
        int int_argc      = argc;
        int int_argv_bytes = 0;
        if (__count_args_bytes(int_argv, int_argc, &int_argv_bytes) < 0) {
            pr_err("sys_execve failed: interpreter arguments exceed ARG_MAX.\n");
            // Rollback: the old image is still the current one.
            kfree(int_argv);
            mm_destroy(new_mm);
            current->uid = prev_uid;
            current->gid = prev_gid;
            kfree(argv_locations);
            kfree(envp_locations);
            kfree(args_mem);
            return -E2BIG;
        }
        void *int_args_mem = kmalloc(int_argv_bytes + envp_bytes);
        if (!int_args_mem) {
            pr_err(
                "Failed to allocate memory for interpreter arguments and "
                "environment %d (%d + %d).\n",
                int_argv_bytes + envp_bytes, int_argv_bytes, envp_bytes);
            // Rollback: the old image is still the current one.
            kfree(int_argv);
            mm_destroy(new_mm);
            current->uid = prev_uid;
            current->gid = prev_gid;
            kfree(argv_locations);
            kfree(envp_locations);
            kfree(args_mem);
            return -ENOMEM;
        }
        // Copy the arguments (kernel strings: lengths were validated on copy).
        uint32_t int_args_mem_ptr = (uint32_t)int_args_mem + (int_argv_bytes + envp_bytes);
        __push_strings_on_stack(&int_args_mem_ptr, int_argv, int_argc, argv_locations);
        saved_argv                = __push_vector_on_stack(&int_args_mem_ptr, argv_locations, int_argc);
        __push_strings_on_stack(&int_args_mem_ptr, saved_envp, envc, envp_locations);
        saved_envp                = __push_vector_on_stack(&int_args_mem_ptr, envp_locations, envc);
        // Check the memory pointer.
        assert(int_args_mem_ptr == (uint32_t)int_args_mem);
        // Free the interpreter argv array and the old argument and environ memory block.
        kfree(int_argv);
        kfree(args_mem);
        args_mem = int_args_mem;
    }
    // ------------------------------------------------------------------------

    // == INITIALIZE PROGRAM ARGUMENTS ========================================
    // Save the current page directory.
    page_directory_t *crtdir = paging_get_current_pgd();

    // Temporarily switch to the page directory of the candidate image to
    // initialize its stack.
    paging_switch_pgd(new_mm->pgd);

    // The stack of the new image starts at its top.
    uintptr_t useresp = new_mm->start_stack + DEFAULT_STACK_SIZE;
    // Save where the arguments start.
    new_mm->arg_start = useresp;
    // Push the arguments on the stack (kernel strings, argc reflects the
    // interpreter shift when a script was loaded).
    final_argv        = __push_args_on_stack(&useresp, saved_argv, argc, argv_locations);
    // Save where the arguments end, and the env starts.
    new_mm->env_start = new_mm->arg_end = useresp;
    // Push the environment on the stack.
    final_envp                           = __push_args_on_stack(&useresp, saved_envp, envc, envp_locations);
    // Save where the environmental variables end.
    new_mm->env_end                      = useresp;
    // The string positions are no longer needed.
    kfree(argv_locations);
    kfree(envp_locations);
    // Push the `main` arguments on the stack (argc, argv, envp).
    stack_push_ptr(&useresp, final_envp);
    stack_push_ptr(&useresp, final_argv);
    stack_push_s32(&useresp, argc);

    // Restore previous pgdir
    paging_switch_pgd(crtdir);
    // ------------------------------------------------------------------------

    // == COMMIT ===============================================================
    // The candidate image is complete: install it on the task, destroy the
    // old image, and set the registers of the new image. Past this point
    // the syscall cannot fail anymore.
    mm_struct_t *old_mm     = current->mm;
    current->mm             = new_mm;
    if (old_mm != NULL) {
        mm_destroy(old_mm);
    }
    // Set the entry point.
    current->thread.regs.eip     = entry;
    // Set the base address and the top of the stack.
    current->thread.regs.ebp     = useresp;
    current->thread.regs.useresp = useresp;
    // Enable the interrupts.
    current->thread.regs.eflags  = current->thread.regs.eflags | EFLAG_IF;

    // Change the name of the process. name_buffer is bounded and terminated
    // above; the bounded copy keeps this safe even if that ever changes.
    strncpy(current->name, name_buffer, sizeof(current->name) - 1);
    current->name[sizeof(current->name) - 1] = '\0';

    // Free the temporary args memory.
    kfree(args_mem);

    // Perform the switch to the new process.
    scheduler_restore_context(current, f);

    pr_debug("Executing '%s' (pid: %d)...\n", current->name, current->pid);

    return 0;
}
