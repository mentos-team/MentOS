/// @file tss.h
/// @brief Data structures concerning the Task State Segment (TSS).
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
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
typedef struct tss_entry {
    uint32_t prev_tss; ///< If we used hardware task switching this would form a linked list.
    uint32_t esp0;     ///< The stack pointer to load when we change to kernel mode.
    uint32_t ss0;      ///< The stack segment to load when we change to kernel mode.
    uint32_t esp1;     ///< everything below here is unusued now.
    uint32_t ss1;      ///< TODO: Comment.
    uint32_t esp2;     ///< TODO: Comment.
    uint32_t ss2;      ///< TODO: Comment.
    uint32_t cr3;      ///< TODO: Comment.
    uint32_t eip;      ///< TODO: Comment.
    uint32_t eflags;   ///< TODO: Comment.
    uint32_t eax;      ///< TODO: Comment.
    uint32_t ecx;      ///< TODO: Comment.
    uint32_t edx;      ///< TODO: Comment.
    uint32_t ebx;      ///< TODO: Comment.
    uint32_t esp;      ///< TODO: Comment.
    uint32_t ebp;      ///< TODO: Comment.
    uint32_t esi;      ///< TODO: Comment.
    uint32_t edi;      ///< TODO: Comment.
    uint32_t es;       ///< TODO: Comment.
    uint32_t cs;       ///< TODO: Comment.
    uint32_t ss;       ///< TODO: Comment.
    uint32_t ds;       ///< TODO: Comment.
    uint32_t fs;       ///< TODO: Comment.
    uint32_t gs;       ///< TODO: Comment.
    uint32_t ldt;      ///< TODO: Comment.
    uint16_t trap;     ///< TODO: Comment.
    uint16_t iomap;    ///< TODO: Comment.
} tss_entry_t;

/// @brief Flushes the Task State Segment.
extern void tss_flush();

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
