/// @file panic.c
/// @brief Functions used to manage kernel panic.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/panic.h"
#include "io/debug.h"
#include "io/port_io.h"

/// Set for every non-interactive boot mode: see kernel.c.
extern int qemu_exit_on_panic;

void kernel_panic(const char *msg)
{
    pr_emerg("\nPANIC:\n%s\n\nWelcome to Kernel Debugging Land...\n", msg);
    __asm__ __volatile__("cli"); // Disable interrupts
    if (qemu_exit_on_panic) {
        // Signal failure via isa-debug-exit (guest write 0x11 → host exit 35)
        outports(DEBUG_EXIT_PORT, DEBUG_EXIT_FAILURE);
    }
    for (;;) {
        // Decrease power consumption with hlt.
        __asm__ __volatile__("hlt");
    }
}
