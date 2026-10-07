/// @file   boot_cpu.h
/// @brief  Minimal control-register access used by the bootloader.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

#define BOOT_CR0_PG  0x80000000U
#define BOOT_CR4_PSE 0x00000010U

static inline uintptr_t boot_get_cr0(void)
{
    uintptr_t value;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(value));
    return value;
}

static inline void boot_set_cr0(uintptr_t value) { __asm__ __volatile__("mov %0, %%cr0" : : "r"(value)); }
static inline void boot_set_cr3(uintptr_t value) { __asm__ __volatile__("mov %0, %%cr3" : : "r"(value)); }

static inline uintptr_t boot_get_cr4(void)
{
    uintptr_t value;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(value));
    return value;
}

static inline void boot_set_cr4(uintptr_t value)
{
    __asm__ __volatile__("mov %0, %%cr4" : : "r"(value) : "memory");
}
