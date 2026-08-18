/// @file panic.c
/// @brief Functions used to manage kernel panic.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/panic.h"
#include "io/debug.h"
#include "io/port_io.h"

/// Debug exit port for QEMU isa-debug-exit device (default iobase=0x501)
/// Exit code encoding: host_exit = (guest_value << 1) | 1
#define DEBUG_EXIT_PORT 0x501
extern int runtests;

void kernel_panic(const char *msg)
{
    pr_emerg("\nPANIC:\n%s\n\nWelcome to Kernel Debugging Land...\n", msg);
    __asm__ __volatile__("cli"); // Disable interrupts
    if (runtests) {
        // Signal failure via isa-debug-exit (guest write 0x11 → host exit 35)
        outports(DEBUG_EXIT_PORT, 0x11);
    }
    for (;;) {
        // Decrease power consumption with hlt.
        __asm__ __volatile__("hlt");
    }
}
