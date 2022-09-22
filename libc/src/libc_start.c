/// @file libc_start.c
/// @brief Contains the programs initialization procedure.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"
#include "system/syscall_types.h"
#include "sys/errno.h"

/// @brief Reference to the `environ` variable in `setenv.c`.
extern char **environ;

/// @brief The entry point to every program.
/// @param main Pointer to the main function.
/// @param argc The number of arguments.
/// @param argv The pointer to the arguments.
/// @param envp The pointer to the environmental variables.
/// @return The return of the `main` function.
int __libc_start_main(int (*main)(int, char **, char **), int argc, char *argv[], char *envp[])
{
    //dbg_print("== START %-30s =======================================\n", argv[0]);
    //dbg_print("__libc_start_main(%p, %d, %p, %p)\n", main, argc, argv, envp);
    assert(main && "There is no `main` function.");
    assert(argv && "There is no `argv` array.");
    assert(envp && "There is no `envp` array.");
    //dbg_print("environ  : %p\n", environ);
    // Copy the environ.
    environ = envp;
    // Call the main function.
    int result = main(argc, argv, envp);
    // Free the environ.
    //dbg_print("== END   %-30s =======================================\n", argv[0]);
    return result;
}
