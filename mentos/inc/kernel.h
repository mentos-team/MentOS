/// @file   kernel.h
/// @brief  Kernel generic data structure and functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// @brief The initial stack pointer.
extern uintptr_t initial_esp;

/// Kilobytes.
#define K 1024
/// Megabytes.
#define M (1024 * K)
/// Gigabytes.
#define G (1024 * M)

/// @brief Interrupt stack frame.
/// @details
/// When the CPU moves from Ring3 to Ring0 because of an interrupt,
/// the following registes/values are moved into the kernel's stack
typedef struct pt_regs {
    /// FS and GS have no hardware-assigned uses.
    uint32_t gs;
    /// FS and GS have no hardware-assigned uses.
    uint32_t fs;
    /// Extra Segment determined by the programmer.
    uint32_t es;
    /// Data Segment.
    uint32_t ds;
    /// 32-bit destination register.
    uint32_t edi;
    /// 32-bit source register.
    uint32_t esi;
    /// 32-bit base pointer register.
    uint32_t ebp;
    /// 32-bit stack pointer register.
    uint32_t esp;
    /// 32-bit base register.
    uint32_t ebx;
    /// 32-bit data register.
    uint32_t edx;
    /// 32-bit counter.
    uint32_t ecx;
    /// 32-bit accumulator register.
    uint32_t eax;
    /// Interrupt number.
    uint32_t int_no;
    /// Error code.
    uint32_t err_code;
    /// Instruction Pointer Register.
    uint32_t eip;
    /// Code Segment.
    uint32_t cs;
    /// 32-bit flag register.
    uint32_t eflags;
    /// User application ESP.
    uint32_t useresp;
    /// Stack Segment.
    uint32_t ss;
} pt_regs;
