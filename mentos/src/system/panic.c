///                MentOS, The Mentoring Operating system project
/// @file panic.c
/// @brief Functions used to manage kernel panic.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "panic.h"
#include "debug.h"

void kernel_panic(const char *msg)
{
    pr_emerg("\nPANIC:\n%s\n\nWelcome to Kernel Debugging Land...\n\n", msg);
    pr_emerg("\n");
    asm("cli"); // Disable interrupts
    for (;;) asm("hlt"); // Decrease power consumption with hlt
}
