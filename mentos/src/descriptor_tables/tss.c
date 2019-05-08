///                MentOS, The Mentoring Operating system project
/// @file tss.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "tss.h"
#include "debug.h"
#include "string.h"

static tss_entry_t kernel_tss;

void tss_init(uint8_t idx, uint32_t kss, uint32_t kesp)
{
    uint32_t base = (uint32_t) &kernel_tss;
    uint32_t limit = base + sizeof(tss_entry_t);

    // Add the TSS descriptor to the GDT.
    // Kernel tss, access(E9 = 1 11 0 1 0 0 1)
    //    1   present
    //    11  ring 3
    //    0   should always be 1, why 0? may be this value doesn't matter at all
    //    1   code?
    //    0   can not be executed by ring lower or equal to DPL,
    //    0   not readable
    //    1   access bit, always 0, cpu set this to 1 when accessing this
    //    sector(why 0 now?)
    gdt_set_gate(idx, base, limit, 0xE9, 0x0);

    // init. tss entry to zero
    memset(&kernel_tss, 0x0, sizeof(tss_entry_t));

    kernel_tss.ss0 = kss;
    // Note that we usually set tss's esp to 0 when booting our os, however,
    // we need to set it to the real esp when we've switched to usermode
    // because the CPU needs to know what esp to use when usermode app is
    // calling a kernel function(aka system call), that's why we have a
    // function below called tss_set_stack.
    kernel_tss.esp0 = kesp;
    kernel_tss.cs = 0x0b;
    kernel_tss.ds = 0x13;
    kernel_tss.es = 0x13;
    kernel_tss.fs = 0x13;
    kernel_tss.gs = 0x13;
    kernel_tss.ss = 0x13;
    kernel_tss.iomap = sizeof(tss_entry_t);
}

void print_tss() {
    printf("TSS: SSO(%p) ESP0(%p)\n", kernel_tss.ss0, kernel_tss.esp0);
}

void tss_set_stack(uint32_t kss, uint32_t kesp)
{
    // Kernel data segment.
    kernel_tss.ss0  = kss;
    // Kernel stack address.
    kernel_tss.esp0 = kesp;
}
