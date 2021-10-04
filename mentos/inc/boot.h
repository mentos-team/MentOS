///                MentOS, The Mentoring Operating system project
/// @file boot.h
/// @brief Bootloader structures
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#ifndef MENTOS_BOOT_H
#define MENTOS_BOOT_H

#include "multiboot.h"

/// @brief Mentos structure to communicate bootloader info to the kernel
typedef struct boot_info_t {
    /// Boot magic number.
    uint32_t magic;

    /*
     * Bootloader physical range
     * it can be overwritten to do other things
     * */

    /// bootloader code start
    uint32_t bootloader_phy_start;
    /// bootloader code end
    uint32_t bootloader_phy_end;

    /*
     * Kernel virtual and physical range
     * must not be overwritten
     * it's assumed to be contiguous, so section holes are just wasted space
     * */

    /// kernel physical code start
    uint32_t kernel_phy_start;
    /// kernel physical code end
    uint32_t kernel_phy_end;

    /// kernel code start
    uint32_t kernel_start;
    /// kernel code end
    uint32_t kernel_end;

    /// kernel size.
    uint32_t kernel_size;

    /// Address after the modules.
    uint32_t module_end;

    /*
     * Range of addressable lowmemory, is memory that is available
     * (not used by the kernel executable nor by the bootloader)
     * and can be accessed by the kernel at any time
     * */

    /// lowmem physical addressable start
    uint32_t lowmem_phy_start;
    /// lowmem physical addressable end
    uint32_t lowmem_phy_end;

    /// lowmem addressable start
    uint32_t lowmem_start;
    /// lowmem addressable end
    uint32_t lowmem_end;

    /// stack end (comes after lowmem_end, and is the end of the low mapped memory)
    uint32_t stack_end;

    /*
     * Range of non-addressable highmemory, is memory that can be
     * accessed by the kernel only if previously mapped
     * */

    /// highmem addressable start
    uint32_t highmem_phy_start;
    /// highmem addressable end
    uint32_t highmem_phy_end;

    /// multiboot info
    multiboot_info_t *multiboot_header;

    /// stack suggested start address (also set by the bootloader)
    uint32_t stack_base;
} boot_info_t;

#endif //MENTOS_BOOT_H
