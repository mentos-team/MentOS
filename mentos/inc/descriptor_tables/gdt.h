/// @file gdt.h
/// @brief Data structures concerning the Global Descriptor Table (GDT).
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup descriptor_tables Descriptor Tables
/// @brief GDT, IDT and TSS are all data structures specified by Intel x86
/// architecture in memory management module.
/// @{
/// @addtogroup gdt Global Descriptor Table (GDT)
/// @brief Is used to define the characteristics of the various memory areas
/// used during program execution, including the base address, the size and
/// access privileges like executability and writability.
/// @{

#pragma once

#include "stdint.h"

/// @brief Bitmasks used to access to specific bits of the GDT.
/// @details
/// @image html gdt_bits.png
/// #### PRIV (Privilege bits)
/// Contains the ring level, specifically:
///   - `00 (0)` = highest (kernel),
///   - `11 (3)` = lowest (user applications).
///
/// #### S (Descriptor type)
/// This bit should be set for code or data segments and should be cleared
/// for system segments (eg. a Task State Segment).
///
/// #### EX (Executable bit)
///   - If 1 code in this segment can be executed, ie. a code selector.
///   - If 0 it is a data selector.
///
/// #### DC (Direction bit/Conforming bit)
/// Direction bit for data selectors: Tells the direction. 0 the segment grows up.
/// 1 the segment grows down, ie., the offset has to be greater than the limit.
///
/// Conforming bit for code selectors:
///  - If 1 code in this segment can be executed from an equal or lower privilege
///    level. For example, code in ring 3 can far-jump to conforming code in a ring 2 segment.
///    The privl-bits represent the highest privilege level that is allowed to execute
///    the segment. For example, code in ring 0 cannot far-jump to a conforming code segment
///    with privl==0x2, while code in ring 2 and 3 can. Note that the privilege level remains
///    the same, ie. a far-jump form ring 3 to a privl==2-segment remains in ring 3
///    after the jump.
///  - If 0 code in this segment can only be executed from the ring set in privl.
///
/// #### RW (Readable bit/Writable bit)
///  - Readable bit for code selectors:
///    - Whether read access for this segment is allowed.
///    - Write access is never allowed for code segments.
///  - Writable bit for data selectors:
///    - Whether write access for this segment is allowed.
///    - Read access is always allowed for data segments.
///
/// #### AC (Accessed bit)
/// Just set to 0. The CPU sets this to 1 when the segment is accessed.
///
/// #### GR (Granularity bit)
///  - If 0 the limit is in 1 B blocks (byte granularity);
///  - if 1 the limit is in 4 KiB blocks (page granularity).
///
/// #### SZ (Size bit)
///  - If 0 the selector defines 16 bit protected mode;
///  - if 1 it defines 32 bit protected mode.
///
///  You can have both 16 bit and 32 bit selectors at once.
///
enum gdt_bits_t {
    /// @brief `0b10000000U` (Present): This must be 1 for all valid selectors.
    GDT_PRESENT = 128U,
    /// @brief `0b00000000U` (Privilege): Sets the 2 privilege bits (ring level) to 0 = highest (kernel).
    GDT_KERNEL = 0U,
    /// @brief `0b01100000U` (Privilege): Sets the 2 privilege bits (ring level) to 3 = lowest (user applications).
    GDT_USER = 96U,
    /// @brief `0b00010000U` (Descriptor): This bit should be set for code or data segments and should be cleared for system segments (eg. a Task State Segment)
    GDT_S = 16U,
    /// @brief `0b00001000U` (Executable): If 1 code in this segment can be executed, ie. a code selector. If 0 it is a data selector.
    GDT_EX = 8U,
    /// @brief `0b00000100U` (Direction/Conforming)
    GDT_DC = 4U,
    /// @brief `0b00000010U` (Readable/Writable)
    GDT_RW = 2U,
    /// @brief `0b00000001U` (Accessed): Just set to 0. The CPU sets this to 1 when the segment is accessed.
    GDT_AC = 1U,
    /// @brief `0b00001100U` (Executable Code): Identifies an executable code segment.
    GDT_CODE = (GDT_S | GDT_EX),
    /// @brief `0b00001001U` (Writable Data): Identifies a writable data segment.
    GDT_DATA = (GDT_S | GDT_RW),
    /// @brief `0b10000000U` (Granularity): If 0 the limit is in 1 B blocks (byte granularity), if 1 the limit is in 4 KiB blocks (page granularity).
    GDT_GRANULARITY = 128U,
    /// @brief `0b01000000U` (Size): If 0 the selector defines 16 bit protected mode. If 1 it defines 32 bit protected mode. You can have both 16 bit and 32 bit selectors at once.
    GDT_OPERAND_SIZE = 64U,
};

/// @brief Used in IDT for padding.
#define IDT_PADDING 14U // `0b00001110U

/// @brief Data structure representing a GDT descriptor.
typedef struct gdt_descriptor_t {
    /// The lower 16 bits of the limit.
    uint16_t limit_low;
    /// The lower 16 bits of the base.
    uint16_t base_low;
    /// The next 8 bits of the base.
    uint8_t base_middle;
    /// Access flags, determine what ring this segment can be used in.
    uint8_t access;
    /// SegLimit_hi(4 bit) AVL(1 bit) L(1 bit) D/B(1 bit) G(1bit).
    uint8_t granularity;
    /// The last 8 bits of the base.
    uint8_t base_high;
} __attribute__((packed)) gdt_descriptor_t;

/// @brief Data structure used to load the GDT into the GDTR.
typedef struct gdt_pointer_t {
    /// The size of the GDT (entry number).
    uint16_t limit;
    /// The starting address of the GDT.
    uint32_t base;
} __attribute__((packed)) gdt_pointer_t;

/// @brief   Initialise the GDT.
/// @details This will setup the special GDT
///          pointer, set up the first 3 entries in our GDT, and then
///          finally call gdt_flush() in our assembler file in order
///          to tell the processor where the new GDT is and update the
///          new segment registers.
void init_gdt();

/// @brief          Sets the value of one GDT entry.
/// @param index    The index inside the GDT.
/// @param base     Memory address where the segment we are defining starts.
/// @param limit    The memory address which determines the end of the segnment.
/// @param access  Type (4bit) - S (1) bit -DPL (2 bit) - P(1 bit).
/// @param granul  SegLimit_hi(4 bit) AVL(1 bit) L(1 bit) D/B(1 bit) G(1bit).
void gdt_set_gate(uint8_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granul);

/// @}
/// @}
