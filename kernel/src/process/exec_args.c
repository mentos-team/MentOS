/// @file exec_args.c
/// @brief The argument and environment vectors of an execve.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EXECARG]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "process/exec_args.h"

#include "assert.h"
#include "errno.h"
#include "klib/stack_helper.h"
#include "limits.h"
#include "mem/alloc/slab.h"
#include "mem/uaccess.h"
#include "string.h"

/// @brief Counts the number of arguments of a vector the kernel owns.
/// @param args the array of arguments, it must be NULL terminated.
/// @param max_count the maximum number of entries to scan.
/// @return the number of arguments, or -E2BIG when the vector is not
///         NULL-terminated within `max_count` entries.
/// @details Only for vectors the kernel built itself. One that came from the
///          caller goes through __count_user_args, which proves every slot
///          before it reads it (#396).
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

/// @brief Counts the number of arguments of a vector in the caller's memory.
/// @param args the array of arguments, straight from user memory.
/// @param max_count the maximum number of entries to scan.
/// @return the number of arguments, -EFAULT when a slot of the vector is not
///         the caller's memory, or -E2BIG when the vector is not
///         NULL-terminated within `max_count` entries.
/// @details Each slot is proven before it is read. Reading it to find out
///          whether it is NULL is already the dereference the check exists to
///          gate, which is the ordering the argv[0] path got wrong before
///          #387. Only the slots are covered here: the strings they point at
///          are ranges of their own, proven by __count_user_args_bytes.
static inline int __count_user_args(char **args, int max_count)
{
    for (int argc = 0; argc <= max_count; ++argc) {
        if (!access_ok(USER_READ, &args[argc], sizeof(args[argc]))) {
            return -EFAULT;
        }
        if (args[argc] == NULL) {
            return argc;
        }
    }
    return -E2BIG;
}

/// @brief Turns a character total into the size of the block that holds a
///        vector: the strings, plus the pointer array and its terminator.
/// @param nchar the string bytes, each including its terminator.
/// @param argc the number of arguments.
/// @param out_bytes where the total is stored.
/// @return 0 on success, -E2BIG when the total exceeds ARG_MAX.
static inline int __args_block_size(int nchar, int argc, int *out_bytes)
{
    *out_bytes = nchar + ((argc + 1 /* The NULL terminator */) * (int)sizeof(char *));
    if (*out_bytes > ARG_MAX) {
        return -E2BIG;
    }
    return 0;
}

/// @brief Counts the bytes occupied by the arguments of a vector the kernel
///        owns.
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
    return __args_block_size(nchar, argc, out_bytes);
}

/// @brief Counts the bytes occupied by the arguments of a vector in the
///        caller's memory.
/// @param args the array of arguments, straight from user memory.
/// @param argc the number of arguments, already validated by
///        __count_user_args.
/// @param out_bytes where the total is stored: the string bytes (each
///        including its terminator) plus the pointer array.
/// @return 0 on success, -EFAULT when a page of a string is not the caller's
///         memory, or -E2BIG when a string is not NUL-terminated within
///         MAX_ARG_STRLEN bytes or the total exceeds ARG_MAX.
/// @details __count_user_args proved the slots of the vector; the strings
///          they point at are somewhere else entirely and were measured with
///          a plain strnlen until #396, which is a bound on how far the
///          kernel reads and not a check on where it reads from.
static inline int __count_user_args_bytes(char **args, int argc, int *out_bytes)
{
    int nchar = 0;
    for (int i = 0; i < argc; i++) {
        long len = strnlen_user(args[i], MAX_ARG_STRLEN);
        if (len == -ENAMETOOLONG) {
            // No terminator within the per-string bound: the -E2BIG this
            // function reported before it could also fail with -EFAULT.
            return -E2BIG;
        }
        if (len < 0) {
            return (int)len;
        }
        nchar += (int)len + 1;
        if (nchar > ARG_MAX) {
            return -E2BIG;
        }
    }
    return __args_block_size(nchar, argc, out_bytes);
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
/// @param argc the number of arguments, already validated by
///        __count_user_args.
/// @param locations array of at least `argc` entries, caller-owned.
/// @param floor the lowest address the pushes may reach: the strings were
///        counted before, and a string that grew since then must fail with
///        -E2BIG here rather than push more bytes than were accounted for
///        (which would write below the allocation).
/// @return 0 on success, -EFAULT when a page of a string is not the caller's
///         memory, or -E2BIG when a string is not NUL-terminated within
///         MAX_ARG_STRLEN bytes, or the pushes would cross `floor`.
static inline int
__push_user_strings_on_stack(uintptr_t *stack, char *args[], int argc, char *locations[], uintptr_t floor)
{
    for (int i = argc - 1; i >= 0; --i) {
        // Measure first, because the room has to be reserved before the copy
        // can land anywhere. The copy then bounds itself against that room,
        // so a string that grew since it was counted fails here (#396).
        long len = strnlen_user(args[i], MAX_ARG_STRLEN);
        if (len == -EFAULT) {
            return -EFAULT;
        }
        if (len < 0) {
            return -E2BIG;
        }
        if ((*stack - ((uintptr_t)len + 1)) < floor) {
            return -E2BIG;
        }
        *stack -= (uintptr_t)len + 1;
        if (strncpy_from_user((char *)(*stack), args[i], (size_t)len + 1) < 0) {
            return -E2BIG;
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

char **exec_args_push_vector(uintptr_t *stack, char *args[], int argc, char *locations[])
{
    __push_strings_on_stack(stack, args, argc, locations);
    return __push_vector_on_stack(stack, locations, argc);
}

int exec_args_from_user(exec_args_t *args, char **argv, char **envp)
{
    // The environment is either the caller's memory or the default the kernel
    // installs below. The two take different helpers: the caller's vector is
    // proven entry by entry, the kernel's one must not be, because none of it
    // is the caller's memory and every check would refuse it (#396).
    int envp_is_user = 1;
    if (envp == NULL) {
        // We allow a NULL environment, using a default, for macOS compatibility
        pr_debug("NULL envp, using default environment.\n");
        static char *default_env[] = {
            "PATH=/bin:/usr/bin",
            "HOME=/",
            NULL};
        envp         = default_env;
        envp_is_user = 0;
    }

    memset(args, 0, sizeof(*args));

    // Every count is bounded: a vector that is not NULL-terminated within
    // MAX_ARG_COUNT entries, a string without a terminator within
    // MAX_ARG_STRLEN bytes, or an argv/envp above ARG_MAX fails with -E2BIG,
    // instead of walking user memory unbounded and overflowing kernel state
    // (#196).
    int argc = __count_user_args(argv, MAX_ARG_COUNT);
    if (argc < 0) {
        pr_err("argv is unreadable, or has too many entries.\n");
        return argc;
    }
    int envc = envp_is_user ? __count_user_args(envp, MAX_ARG_COUNT) : __count_args(envp, MAX_ARG_COUNT);
    if (envc < 0) {
        pr_err("envp is unreadable, or has too many entries.\n");
        return envc;
    }
    int argv_bytes = 0;
    int envp_bytes = 0;
    int result     = __count_user_args_bytes(argv, argc, &argv_bytes);
    if (result < 0) {
        pr_err("argv is unreadable, or exceeds ARG_MAX.\n");
        return result;
    }
    result = envp_is_user ? __count_user_args_bytes(envp, envc, &envp_bytes)
                          : __count_args_bytes(envp, envc, &envp_bytes);
    if (result < 0) {
        pr_err("envp is unreadable, or exceeds ARG_MAX.\n");
        return result;
    }

    args->argc       = argc;
    args->envc       = envc;
    args->argv_bytes = argv_bytes;
    args->envp_bytes = envp_bytes;
    args->block      = kmalloc(argv_bytes + envp_bytes);
    if (!args->block) {
        pr_err("Failed to allocate memory for arguments and environment %d (%d + %d).\n", argv_bytes + envp_bytes, argv_bytes, envp_bytes);
        return -ENOMEM;
    }
    // The arrays of string positions are sized from the validated counts: the
    // argv one also covers the interpreter path, which shifts argv by two
    // entries and therefore needs argc + 2 slots.
    args->argv_locations = kmalloc((argc + 2) * sizeof(char *));
    args->envp_locations = kmalloc(((envc > 0) ? envc : 1) * sizeof(char *));
    if (!args->argv_locations || !args->envp_locations) {
        pr_err("Failed to allocate memory for the argument positions.\n");
        exec_args_free(args);
        return -ENOMEM;
    }

    // Copy the strings (raw user strings, bounded per string and against the
    // total budget: one that grew after the counting fails here instead of
    // being copied past what was accounted for). The argv strings must stay
    // above the environment region of the block.
    uintptr_t cursor = (uintptr_t)args->block + (argv_bytes + envp_bytes);
    result           = __push_user_strings_on_stack(&cursor, argv, argc, args->argv_locations, (uintptr_t)args->block + envp_bytes);
    if (result < 0) {
        pr_err("An argument is unreadable, or is not terminated within the limit.\n");
        exec_args_free(args);
        return result;
    }
    args->argv = __push_vector_on_stack(&cursor, args->argv_locations, argc);
    if (envp_is_user) {
        result = __push_user_strings_on_stack(&cursor, envp, envc, args->envp_locations, (uintptr_t)args->block);
        if (result < 0) {
            pr_err("An environment entry is unreadable, or is not terminated within the limit.\n");
            exec_args_free(args);
            return result;
        }
    } else {
        __push_strings_on_stack(&cursor, envp, envc, args->envp_locations);
    }
    args->envp = __push_vector_on_stack(&cursor, args->envp_locations, envc);
    // The block was sized from the counts, so the pushes must have consumed
    // exactly all of it.
    assert(cursor == (uintptr_t)args->block);
    return 0;
}

int exec_args_insert_interpreter(exec_args_t *args, const char *script)
{
    // The interpreter receives the script as its second argument, so every
    // entry from the first one on shifts right by one.
    char **interpreter_argv = kmalloc((args->argc + 2) * sizeof(char *));
    if (!interpreter_argv) {
        pr_err("Failed to allocate memory for interpreter argv array.\n");
        return -ENOMEM;
    }
    interpreter_argv[0] = args->argv[0]; // TODO: pass the path to the interpreter.
    interpreter_argv[1] = (char *)script;
    for (int i = 1; i <= args->argc; i++) {
        interpreter_argv[i + 1] = args->argv[i];
    }
    int interpreter_argc = args->argc + 1;

    // Rebuild the block. It must hold both the new argv and the whole
    // environment (#227), which is copied over from the old one.
    int interpreter_bytes = 0;
    if (__count_args_bytes(interpreter_argv, interpreter_argc, &interpreter_bytes) < 0) {
        pr_err("Interpreter arguments exceed ARG_MAX.\n");
        kfree(interpreter_argv);
        return -E2BIG;
    }
    void *block = kmalloc(interpreter_bytes + args->envp_bytes);
    if (!block) {
        pr_err("Failed to allocate memory for interpreter arguments and environment %d (%d + %d).\n", interpreter_bytes + args->envp_bytes, interpreter_bytes, args->envp_bytes);
        kfree(interpreter_argv);
        return -ENOMEM;
    }
    // Both vectors are kernel strings by now: their lengths were validated
    // when they were copied in.
    uintptr_t cursor = (uintptr_t)block + (interpreter_bytes + args->envp_bytes);
    __push_strings_on_stack(&cursor, interpreter_argv, interpreter_argc, args->argv_locations);
    char **new_argv = __push_vector_on_stack(&cursor, args->argv_locations, interpreter_argc);
    __push_strings_on_stack(&cursor, args->envp, args->envc, args->envp_locations);
    char **new_envp = __push_vector_on_stack(&cursor, args->envp_locations, args->envc);
    assert(cursor == (uintptr_t)block);

    kfree(interpreter_argv);
    kfree(args->block);
    args->block      = block;
    args->argv       = new_argv;
    args->envp       = new_envp;
    args->argc       = interpreter_argc;
    args->argv_bytes = interpreter_bytes;
    return 0;
}

char **exec_args_push_argv(exec_args_t *args, uintptr_t *stack)
{
    return exec_args_push_vector(stack, args->argv, args->argc, args->argv_locations);
}

char **exec_args_push_envp(exec_args_t *args, uintptr_t *stack)
{
    return exec_args_push_vector(stack, args->envp, args->envc, args->envp_locations);
}

void exec_args_free(exec_args_t *args)
{
    kfree(args->argv_locations);
    kfree(args->envp_locations);
    kfree(args->block);
    memset(args, 0, sizeof(*args));
}
