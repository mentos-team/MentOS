/// @file exec_args.h
/// @brief The argument and environment vectors of an execve.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// @details An execve has to carry argv and envp across the moment the old
/// image is discarded, so both vectors are copied into a kernel block first
/// and laid out on the new image's stack afterwards. That block, and the two
/// scratch arrays that record where each string landed in it, used to be
/// three allocations held by hand inside sys_execve, released by three kfree
/// calls repeated on every error path. Two of those paths released only one
/// of the three. Here the block is owned by exec_args_t and released by one
/// call, so a new error path cannot get the set wrong (#405).
///
/// The vectors also come in two kinds, and the difference is not cosmetic: a
/// vector the caller supplied is proven entry by entry and string by string
/// (#396), while one the kernel built itself — the default environment, or
/// the vector assembled for a shebang — is not the caller's memory and every
/// such check would refuse it. Which one is in hand is decided once, when the
/// arguments are taken in, and never asked again.

#pragma once

#include "stdint.h"

/// @brief The arguments and the environment of an execve, copied out of the
///        caller's memory and owned by the kernel.
typedef struct exec_args {
    void *block;           ///< The allocation holding the strings and both vectors.
    char **argv;           ///< The kernel copy of argv, pointing inside `block`.
    char **envp;           ///< The kernel copy of envp, pointing inside `block`.
    int argc;              ///< The number of entries of `argv`.
    int envc;              ///< The number of entries of `envp`.
    int argv_bytes;        ///< The bytes of `block` that `argv` occupies.
    int envp_bytes;        ///< The bytes of `block` that `envp` occupies.
    char **argv_locations; ///< Where each argv string landed, one entry per argument.
    char **envp_locations; ///< Where each envp string landed, one entry per entry.
} exec_args_t;

/// @brief Takes the arguments and the environment out of the caller's memory.
/// @param args the descriptor to fill; untouched when the call fails.
/// @param argv the caller's argument vector.
/// @param envp the caller's environment vector, or NULL to use the default
///        environment the kernel supplies.
/// @return 0 on success, -EFAULT when a slot of a vector or a page of a
///         string is not the caller's memory, -E2BIG when a vector or a
///         string exceeds its bound, or -ENOMEM.
/// @details On success the caller owns `args` and must release it with
///          exec_args_free, including on every later failure.
int exec_args_from_user(exec_args_t *args, char **argv, char **envp);

/// @brief Rebuilds the arguments for an interpreter, inserting the script
///        path as the second entry and shifting the rest to the right.
/// @param args the arguments to rebuild, filled by exec_args_from_user.
/// @param script the path of the script, copied into the new block.
/// @return 0 on success, -E2BIG when the result exceeds ARG_MAX, or -ENOMEM.
/// @details `args` is left usable and unchanged when the call fails, so the
///          caller can release it the same way either way.
int exec_args_insert_interpreter(exec_args_t *args, const char *script);

/// @brief Lays the arguments out on a stack, growing downwards.
/// @param args the arguments.
/// @param stack pointer to the stack location, updated as the push proceeds.
/// @return the position of the pointer array, which is what `main` receives.
char **exec_args_push_argv(exec_args_t *args, uintptr_t *stack);

/// @brief Lays the environment out on a stack, growing downwards.
/// @param args the arguments.
/// @param stack pointer to the stack location, updated as the push proceeds.
/// @return the position of the pointer array, which is what `main` receives.
char **exec_args_push_envp(exec_args_t *args, uintptr_t *stack);

/// @brief Lays a kernel-owned vector out on a stack, growing downwards.
/// @param stack pointer to the stack location, updated as the push proceeds.
/// @param args the vector; its strings must be kernel copies.
/// @param argc the number of entries.
/// @param locations array of at least `argc` entries, caller-owned, used to
///        record where each string landed.
/// @return the position of the pointer array, which is what `main` receives.
/// @details Exported for the init process, which carries a vector of kernel
///          literals and never goes through exec_args_t at all.
char **exec_args_push_vector(uintptr_t *stack, char *args[], int argc, char *locations[]);

/// @brief Releases everything the arguments own.
/// @param args the arguments; it is left empty, so a second call is harmless.
void exec_args_free(exec_args_t *args);
