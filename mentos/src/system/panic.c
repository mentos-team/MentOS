///                MentOS, The Mentoring Operating system project
/// @file panic.c
/// @brief Functions used to manage kernel panic.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "panic.h"
#include "elf.h"
#include "stdio.h"
#include "kernel.h"
#include "debug.h"

void kernel_panic(const char *msg)
{
    dbg_print("\nPANIC:\n%s\n\nWelcome to Kernel Debugging Land...\n\n", msg);
    dbg_print("\n");
    for (;;);
}
