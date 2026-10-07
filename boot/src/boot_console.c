/// @file boot_console.c
/// @brief Minimal serial diagnostics used before entering the kernel.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "boot_console.h"

#include "stdint.h"

/// COM1 is the serial port configured by the boot test harness and QEMU.
#define SERIAL_COM1 0x03F8U

/// Write one byte to an x86 I/O port.
static inline void boot_outportb(uint16_t port, uint8_t data)
{
    __asm__ __volatile__("outb %%al, %%dx" : : "a"(data), "d"(port));
}

void boot_console_puts(const char *message)
{
    if (!message) {
        return;
    }

    while (*message != '\0') {
        boot_outportb(SERIAL_COM1, (uint8_t)*message++);
    }
}

__attribute__((noreturn)) void boot_fatal(const char *message)
{
    boot_console_puts("[bootloader] FATAL: ");
    boot_console_puts(message);
    boot_console_puts("\n");

    __asm__ __volatile__("cli");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
