/// @file   boot_info.h
/// @brief  ABI passed from the Multiboot loader to the MentOS kernel.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// Forward declaration keeps this contract independent from the Multiboot
/// helper implementation. The concrete structure is supplied by
/// `multiboot.h` on either side of the boot boundary.
typedef struct multiboot_info multiboot_info_t;

/// Version of the boot hand-off layout.
#define BOOT_INFO_ABI_VERSION 1U

/// @brief Information transferred from the bootloader to `kmain`.
/// @details All addresses are 32-bit i386 addresses. Ranges use an inclusive
///          start and an exclusive end unless a field explicitly says
///          otherwise. The kernel copies this structure before replacing the
///          boot page tables.
typedef struct boot_info {
    /// Contract version understood by the kernel.
    uint32_t version;
    /// Number of bytes supplied by the bootloader, including this field.
    uint32_t size;
    /// Multiboot magic received from the firmware loader.
    uint32_t magic;
    /// Physical range occupied by the bootloader image.
    uint32_t bootloader_phy_start;
    uint32_t bootloader_phy_end;
    /// Physical range occupied by the relocated kernel image.
    uint32_t kernel_phy_start;
    uint32_t kernel_phy_end;
    /// Virtual range occupied by the linked kernel image.
    uint32_t kernel_start;
    uint32_t kernel_end;
    /// Size of the virtual kernel range in bytes.
    uint32_t kernel_size;
    /// First physical address after all Multiboot modules.
    uint32_t module_end;
    /// Physical low-memory range initially mapped by the bootloader.
    uint32_t lowmem_phy_start;
    uint32_t lowmem_phy_end;
    /// Virtual low-memory range initially mapped by the bootloader.
    uint32_t lowmem_virt_start;
    uint32_t lowmem_virt_end;
    /// Size of the initially mapped low-memory range in bytes.
    uint32_t lowmem_size;
    /// End of the initial low-memory mapping before stack reservation.
    uint32_t stack_end;
    /// Physical range above the initial low-memory mapping.
    uint32_t highmem_phy_start;
    uint32_t highmem_phy_end;
    /// Original Multiboot information pointer.
    multiboot_info_t *multiboot_header;
    /// Top of the initial kernel stack (the stack grows downward).
    uint32_t stack_base;
    /// Total reserved stack span, including the guard page below it.
    uint32_t stack_size;
} boot_info_t;
