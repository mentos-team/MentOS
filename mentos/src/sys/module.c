/// @file module.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[MODULE]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "mem/alloc/slab.h"
#include "string.h"
#include "sys/bitops.h"
#include "sys/module.h"

/// Defined in kernel.ld, points at the end of kernel's data segment.
extern void *_kernel_end;
/// List of modules.
multiboot_module_t modules[MAX_MODULES];

int init_modules(multiboot_info_t *header)
{
    for (int i = 0; i < MAX_MODULES; ++i) {
        modules[i].mod_start = 0;
        modules[i].mod_end   = 0;
        modules[i].cmdline   = 0;
        modules[i].pad       = 0;
    }
    if (!bitmask_check(header->flags, MULTIBOOT_FLAG_MODS)) {
        return 1;
    }
    multiboot_module_t *mod = first_module(header);
    for (int i = 0; (mod != 0) && (i < MAX_MODULES); ++i, mod = next_module(header, mod)) {
        memcpy(&modules[i], mod, sizeof(multiboot_module_t));
    }
    return 1;
}

int relocate_modules(void)
{
    for (int i = 0; i < MAX_MODULES; ++i) {
        // Exit if modules are finished
        if (!modules[i].mod_start) {
            break;
        }

        // Get module and cmdline sizes
        uint32_t mod_size     = modules[i].mod_end - modules[i].mod_start;
        uint32_t cmdline_size = strlen((const char *)modules[i].cmdline) + 1;

        // Allocate needed memory, to copy both module and command line
        uint32_t memory = (uint32_t)kmalloc(mod_size + cmdline_size);

        if (!memory) {
            return 0;
        }

        // Copy module and its command line
        memcpy((char *)memory, (char *)modules[i].mod_start, mod_size);
        memcpy((char *)memory + mod_size, (char *)modules[i].cmdline, cmdline_size);

        // Change the module address to point to new allocated memory
        modules[i].mod_start = memory;
        modules[i].mod_end = modules[i].cmdline = memory + mod_size;
    }
    return 1;
}

uintptr_t get_address_after_modules(void)
{
    // By default the first valid address is end.
    uintptr_t address_after_modules = (uintptr_t)&_kernel_end;
    for (int i = 0; i < MAX_MODULES; ++i) {
        if (modules[i].mod_start != 0) {
            address_after_modules = modules[i].mod_end;
        }
    }
    return address_after_modules;
}
