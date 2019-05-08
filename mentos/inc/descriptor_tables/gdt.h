///                MentOS, The Mentoring Operating system project
/// @file   gdt.h
/// @brief  Data structures concerning the Global Descriptor Table (GDT).
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// @brief Access flags, determines what ring this segment can be used in.
typedef enum gdt_access_option_t
{
    /// Identifies a kernel segment.
    KERNEL = 0x00,
    /// Identifies a user segment.
    USER = 0x03,
    /// Identifies a code segment.
    CODE = 0x10,
    /// Identifies a data segment.
    DATA = 0x10,
    /// Segment is present.
    PRESENT = 0x80,
} __attribute__ ((__packed__)) gdt_access_option_t;

/// @brief Options for the second option.
typedef enum gdt_granularity_option_t
{
    /// Granularity.
    GRANULARITY = 0x80,
    /// Szbits.
    SZBITS = 0x40
} __attribute__ ((__packed__)) gdt_granularity_option_t;

/// @brief Data structure representing a GDT descriptor.
typedef struct gdt_descriptor_t
{
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
typedef struct gdt_pointer_t
{
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
/// @param _access  Type (4bit) - S (1) bit -DPL (2 bit) - P(1 bit).
/// @param _granul  SegLimit_hi(4 bit) AVL(1 bit) L(1 bit) D/B(1 bit) G(1bit).
void gdt_set_gate(uint8_t index,
                  uint32_t base,
                  uint32_t limit,
                  uint8_t _access,
                  uint8_t _granul);
