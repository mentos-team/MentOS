/// @file panic.c
/// @brief Functions used to manage kernel panic.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/panic.h"
#include "io/debug.h"
#include "io/port_io.h"

#define SHUTDOWN_PORT 0x604
extern int runtests;

void kernel_panic(const char *msg)
{
    pr_emerg("\nPANIC:\n%s\n\nWelcome to Kernel Debugging Land...\n\n", msg);
    pr_emerg("\n");
    __asm__ __volatile__("cli"); // Disable interrupts
    if (runtests) { outports(SHUTDOWN_PORT, 0x2000); } // Terminate qemu running the tests
    for (;;) __asm__ __volatile__("hlt"); // Decrease power consumption with hlt
}
