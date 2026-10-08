#pragma once

/**
 * Write an early-boot diagnostic to the serial console.
 *
 * The bootloader cannot use the kernel logging stack yet, so this tiny
 * interface intentionally has no dependency on kernel or libc headers.
 */
void boot_console_puts(const char *message);

/**
 * Report an unrecoverable boot configuration error and halt the CPU.
 *
 * This function never returns.
 */
__attribute__((noreturn)) void boot_fatal(const char *message);
