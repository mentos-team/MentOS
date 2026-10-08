/// @file boot_module.c
/// @brief Multiboot module discovery for the bootloader.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "boot_module.h"

#include "boot_bits.h"
#include "boot_console.h"
#include "boot_math.h"
#include "stddef.h"

static int boot_is_kernel_module(const multiboot_module_t *module)
{
    static const char expected[] = "kernel";
    const char *command_line;

    if (!module || !module->cmdline) {
        return 0;
    }

    command_line = (const char *)(uintptr_t)module->cmdline;
    for (uint32_t i = 0; expected[i] != '\0'; ++i) {
        if (command_line[i] != expected[i]) {
            return 0;
        }
    }
    return command_line[sizeof(expected) - 1U] == '\0';
}

const unsigned char *boot_get_kernel_image(multiboot_info_t *header, uint32_t *image_size)
{
    multiboot_module_t *kernel_module = NULL;

    if (!header || !image_size || !boot_bit_test(header->flags, MULTIBOOT_FLAG_MODS) || !header->mods_count ||
        !header->mods_addr) {
        boot_fatal("missing kernel module");
    }

    multiboot_module_t *module_table = (multiboot_module_t *)(uintptr_t)header->mods_addr;
    for (uint32_t i = 0; i < header->mods_count; ++i) {
        if (!boot_is_kernel_module(&module_table[i])) {
            continue;
        }
        if (kernel_module) {
            boot_fatal("multiple kernel modules");
        }
        kernel_module = &module_table[i];
    }

    if (!kernel_module) {
        boot_fatal("kernel module not found");
    }
    if (kernel_module->mod_end <= kernel_module->mod_start) {
        boot_fatal("kernel module has an invalid range");
    }

    *image_size = kernel_module->mod_end - kernel_module->mod_start;
    return (const unsigned char *)(uintptr_t)kernel_module->mod_start;
}

uint32_t boot_get_address_after_modules(const multiboot_info_t *header, uint32_t bootloader_end)
{
    uint32_t address = bootloader_end;
    const multiboot_module_t *module = (const multiboot_module_t *)(uintptr_t)header->mods_addr;

    for (uint32_t i = 0; i < header->mods_count; ++i, ++module) {
        address = boot_max(boot_max(address, module->mod_start), module->mod_end);
    }
    return address;
}
