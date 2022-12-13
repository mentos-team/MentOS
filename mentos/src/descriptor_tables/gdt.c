/// @file gdt.c
/// @brief Functions which manage the Global Descriptor Table (GDT).
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[GDT   ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "io/debug.h"
#include "descriptor_tables/gdt.h"
#include "descriptor_tables/tss.h"

/// The maximum dimension of the GDT.
#define GDT_SIZE 10

/// @brief This will be a function in gdt.s. We use this to properly
///        reload the new segment registers
/// @param _gdt_pointer addresss of the gdt.
extern void gdt_flush(uint32_t _gdt_pointer);

/// The GDT itself.
gdt_descriptor_t gdt[GDT_SIZE];

/// Pointer structure to give to the CPU.
gdt_pointer_t gdt_pointer;

void init_gdt()
{
    // BEWARE: Look below for a deeper explanation.

    // Prepare GDT vector.
    for (uint32_t it = 0; it < GDT_SIZE; ++it) {
        gdt[it].limit_low   = 0;
        gdt[it].base_low    = 0;
        gdt[it].base_middle = 0;
        gdt[it].access      = 0;
        gdt[it].granularity = 0;
        gdt[it].base_high   = 0;
    }

    // Setup the GDT pointer and limit.
    // We have six entries in the GDT:
    //  - Two for kernel mode.
    //  - Two for user mode.
    //  - The NULL descriptor.
    //  - And one for the TSS (task state segment).
    // The limit is the last valid byte from the start of the GDT.
    // i.e. the size of the GDT - 1.
    gdt_pointer.limit = sizeof(gdt_descriptor_t) * 6 - 1;
    gdt_pointer.base  = (uint32_t)&gdt;

    // ------------------------------------------------------------------------
    // NULL
    // ------------------------------------------------------------------------
    gdt_set_gate(0, 0, 0, 0, 0);

    // ------------------------------------------------------------------------
    // CODE
    // ------------------------------------------------------------------------
    // The base address is 0, the limit is 4GBytes, it uses 4KByte
    // granularity, uses 32-bit opcodes, and is a Code Segment descriptor.
    gdt_set_gate(
        1,
        0,
        0xFFFFFFFF,
        GDT_PRESENT | GDT_KERNEL | GDT_CODE | GDT_RW,
        GDT_GRANULARITY | GDT_OPERAND_SIZE);

    // ------------------------------------------------------------------------
    // DATA
    // ------------------------------------------------------------------------
    // It's EXACTLY the same as our code segment, but the descriptor type in
    // this entry's access byte says it's a Data Segment.
    gdt_set_gate(
        2,
        0,
        0xFFFFFFFF,
        GDT_PRESENT | GDT_KERNEL | GDT_DATA,
        GDT_GRANULARITY | GDT_OPERAND_SIZE);

    // ------------------------------------------------------------------------
    // USER MODE CODE
    // ------------------------------------------------------------------------
    gdt_set_gate(
        3,
        0,
        0xFFFFFFFF,
        GDT_PRESENT | GDT_USER | GDT_CODE | GDT_RW,
        GDT_GRANULARITY | GDT_OPERAND_SIZE);

    // ------------------------------------------------------------------------
    // USER MODE DATA
    // ------------------------------------------------------------------------
    gdt_set_gate(
        4,
        0,
        0xFFFFFFFF,
        GDT_PRESENT | GDT_USER | GDT_DATA,
        GDT_GRANULARITY | GDT_OPERAND_SIZE);

    // Initialize the TSS
    tss_init(5, 0x10);

    // Inform the CPU about the changes on the GDT.
    gdt_flush((uint32_t)&gdt_pointer);

    // Inform the CPU about the changes on the TSS.
    tss_flush();
}

void gdt_set_gate(uint8_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granul)
{
    // Setup the descriptor base address.
    gdt[index].base_low    = (base & 0xFFFFU);
    gdt[index].base_middle = (base >> 16U) & 0xFFU;
    gdt[index].base_high   = (base >> 24U) & 0xFFU;

    // Setup the descriptor limits.
    gdt[index].limit_low   = (limit & 0xFFFFU);
    gdt[index].granularity = (limit >> 16U) & 0x0FU;

    // Finally, set up the granularity and access flags.
    gdt[index].granularity |= granul & 0xF0U;
    gdt[index].access = access;
    pr_debug(
        "gdt[%2d] = {.low=0x%x, .mid=0x%x, .high=0x%x, .access=0x%x, .granul=0x%x}\n",
        index, gdt[index].base_low, gdt[index].base_middle,
        gdt[index].base_high, gdt[index].access, gdt[index].granularity);
}

//
// == VIRTUAL MEMORY SCHEMES ==================================================
// x86 supports two virtual memory schemes:
//     segmentation (mandatory): managed using the segment table, GDT.
//     paging       (optional) : managed using the page table, PDT.
// Most operating systems want to to use paging and don't want the
// segmentation, but its mandatory and can't just be disabled.
//
// So the trick is to disable its effect as it wasn't there. This can usually
// be done by creating 4 large overlapped segments descriptors (beside the
// null segment):
//     segment index 0 : null segment descriptor
//     segment index 1 : CODE segment desc. for the privileged (kernel) mode
//     segment index 2 : DATA segment desc. for the privileged (kernel) mode
//     segment index 3 : CODE segment desc. for the non-privileged (user) mode
//     segment index 4 : DATA segment desc. for the non-privileged (user) mode
//
// all these segments starts from 0x00000000 up to 0xffffffff, so you end
// up with overlapped large segments that is privileged code and data, and
// non-privileged code and data in the same time. This should open up the
// virtual memory and disable the segmentation effect.
//
// The processor uses the segment selectors (segment registers cs, ds, ss ...)
// to find out the right segment (once again, the segmentation is must).
//
// == SEGMENT SELECTOR ========================================================
// Every segment selector is 16 bit size and has the following layout (source):
//   |15                         3|   2|1   0|
//   |----- Index (13-bit) -----  | TI | RPL |
// where TI is the Table Indicator:
//      0 - GDT
//      1 - LDT
// and RPL encodes in 2 bits the Requestor Privilege Level (RPL):
//     00 - Highest
//     01
//     10
//     11 - Lowest
// Regarding the `privilege level`, x86 supports 4 levels, but only two of them
// are actually used (00 highest, and 11 lowest).
//
// The remaining 13 bits indicates the segment index.
//
// == GDT_FLUSH ===============================================================
// If you look in gdt.S, you will see a `jmp 0x08` at the end of the gdt_flush.
// Now, if you interpret the 0x08 that is loaded in cs, it will be in binary:
//   |               0000000000001|   0|             11|
//   |       index 3 (code)       | GDT|     privileged|
//
// and the 0x10 that is loaded in ds, ss, ... :
//   |               0000000000010|   0|             11|
//   |       index 3 (code)       | GDT|     privileged|
//
// == SS of a USER MODE PROGRAM ===============================================
// If you read the segment selectors of any user mode program you should see
// that the cs value is 27 (0x1b) which means:
//   |               0000000000011|   0|             11|
//   |       index 3 (code)       | GDT| non-privileged|
// and the data selectors ds, ss, ..., should store 35 (0x23):
//   |               0000000000100|   0|             11|
//   |       index 4 (data)       | GDT| non-privileged|
//
