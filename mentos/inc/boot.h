/// @file boot.h
/// @brief Bootloader structures
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "multiboot.h"

/// @brief Mentos structure to communicate bootloader info to the kernel
typedef struct boot_info_t {
    /// Boot magic number.
    unsigned int magic;

    /*
     * Bootloader physical range
     * it can be overwritten to do other things
     * */

    /// bootloader code start
    unsigned int bootloader_phy_start;
    /// bootloader code end
    unsigned int bootloader_phy_end;

    /*
     * Kernel virtual and physical range
     * must not be overwritten
     * it's assumed to be contiguous, so section holes are just wasted space
     * */

    /// kernel physical code start
    unsigned int kernel_phy_start;
    /// kernel physical code end
    unsigned int kernel_phy_end;

    /// kernel code start
    unsigned int kernel_start;
    /// kernel code end
    unsigned int kernel_end;

    /// kernel size.
    unsigned int kernel_size;

    /// Address after the modules.
    unsigned int module_end;

    /*
     * Range of addressable lowmemory, is memory that is available
     * (not used by the kernel executable nor by the bootloader)
     * and can be accessed by the kernel at any time
     * */

    /// lowmem physical addressable start
    unsigned int lowmem_phy_start;
    /// lowmem physical addressable end
    unsigned int lowmem_phy_end;

    /// lowmem addressable start
    unsigned int lowmem_start;
    /// lowmem addressable end
    unsigned int lowmem_end;

    /// stack end (comes after lowmem_end, and is the end of the low mapped memory)
    unsigned int stack_end;

    /*
     * Range of non-addressable highmemory, is memory that can be
     * accessed by the kernel only if previously mapped
     * */

    /// highmem addressable start
    unsigned int highmem_phy_start;
    /// highmem addressable end
    unsigned int highmem_phy_end;

    /// multiboot info
    multiboot_info_t *multiboot_header;

    /// stack suggested start address (also set by the bootloader)
    unsigned int stack_base;
} boot_info_t;
