/// @file tss.h
/// @brief Data structures concerning the Task State Segment (TSS).
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup descriptor_tables Descriptor Tables
/// @{
/// @defgroup tss Task State Segment (TSS)
/// @brief Is a special structure on x86-based computers which holds information
/// about a task. It is used by the operating system kernel for task management.
/// @{

#pragma once

#include "stdint.h"

/// @brief Task state segment entry.
/// @details The layout is fixed by the hardware (Intel SDM vol. 3, "32-Bit
/// Task-State Segment"), so every field is present whether or not it is used.
/// This kernel switches tasks in software, so the processor only ever reads
/// two of them: on a privilege change from user mode to kernel mode it loads
/// `ss0` and `esp0` to find the kernel stack. Everything from `esp1` down is
/// loaded by a hardware task switch, which never happens here, and is kept
/// only so that the structure has the shape the hardware expects.
typedef struct tss_entry {
    uint32_t prev_tss; ///< Selector of the previous TSS; forms a linked list under hardware task switching.
    uint32_t esp0;     ///< The stack pointer to load when we change to kernel mode.
    uint32_t ss0;      ///< The stack segment to load when we change to kernel mode.
    uint32_t esp1;     ///< Stack pointer for ring 1. Unused: this kernel has no ring 1.
    uint32_t ss1;      ///< Stack segment for ring 1. Unused.
    uint32_t esp2;     ///< Stack pointer for ring 2. Unused: this kernel has no ring 2.
    uint32_t ss2;      ///< Stack segment for ring 2. Unused.
    uint32_t cr3;      ///< Page directory base to load on a hardware task switch. Unused.
    uint32_t eip;      ///< Instruction pointer to resume at on a hardware task switch. Unused.
    uint32_t eflags;   ///< Saved flags register. Unused.
    uint32_t eax;      ///< Saved general-purpose register. Unused.
    uint32_t ecx;      ///< Saved general-purpose register. Unused.
    uint32_t edx;      ///< Saved general-purpose register. Unused.
    uint32_t ebx;      ///< Saved general-purpose register. Unused.
    uint32_t esp;      ///< Saved stack pointer. Unused.
    uint32_t ebp;      ///< Saved base pointer. Unused.
    uint32_t esi;      ///< Saved source index. Unused.
    uint32_t edi;      ///< Saved destination index. Unused.
    uint32_t es;       ///< Saved segment selector. Unused.
    uint32_t cs;       ///< Saved code segment selector. Unused.
    uint32_t ss;       ///< Saved stack segment selector. Unused.
    uint32_t ds;       ///< Saved data segment selector. Unused.
    uint32_t fs;       ///< Saved segment selector. Unused.
    uint32_t gs;       ///< Saved segment selector. Unused.
    uint32_t ldt;      ///< Selector of the task's LDT. Unused: this kernel uses no LDT.
    uint16_t trap;     ///< Bit 0 raises a debug exception on a switch to this task. Unused.
    uint16_t iomap;    ///< Offset of the I/O permission bitmap from the base of this TSS.
} tss_entry_t;

/// @brief Flushes the Task State Segment.
extern void tss_flush(void);

/// @brief We don't need tss to assist task switching, but it's required to
///        have one tss for switching back to kernel mode(system call for
///        example).
/// @param idx Index.
/// @param ss0 Kernel data segment.
void tss_init(uint8_t idx, uint32_t ss0);

/// @brief This function is used to set the esp the kernel should be using.
/// @param kss  Kernel data segment.
/// @param kesp Kernel stack address.
void tss_set_stack(uint32_t kss, uint32_t kesp);

/// @}
/// @}
