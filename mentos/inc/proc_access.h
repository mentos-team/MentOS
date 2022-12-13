/// @file   proc_access.h
/// @brief  Set of functions and flags used to manage processors registers.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// Macro that encapsulate asm and volatile directives.
#define ASM(a) __asm__ __volatile__(a)

#define CR0_PE 0x00000001u ///< Protected mode Enable.
#define CR0_MP 0x00000002u ///< "Math" Present (e.g. npx), wait for it.
#define CR0_EM 0x00000004u ///< EMulate NPX, e.g. trap, don't execute code.
#define CR0_TS 0x00000008u ///< Process has done Task Switch, do NPX save.
#define CR0_ET 0x00000010u ///< 32 bit (if set) vs 16 bit (387 vs 287).
#define CR0_PG 0x80000000u ///< Paging Enable.

#define CR4_SEE      0x00008000u ///< Secure Enclave Enable XXX.
#define CR4_SMAP     0x00200000u ///< Supervisor-Mode Access Protect.
#define CR4_SMEP     0x00100000u ///< Supervisor-Mode Execute Protect.
#define CR4_OSXSAVE  0x00040000u ///< OS supports XSAVE.
#define CR4_PCIDE    0x00020000u ///< PCID Enable.
#define CR4_RDWRFSGS 0x00010000u ///< RDWRFSGS Enable.
#define CR4_SMXE     0x00004000u ///< Enable SMX operation.
#define CR4_VMXE     0x00002000u ///< Enable VMX operation.
#define CR4_OSXMM    0x00000400u ///< SSE/SSE2 exception support in OS.
#define CR4_OSFXS    0x00000200u ///< SSE/SSE2 OS supports FXSave.
#define CR4_PCE      0x00000100u ///< Performance-Monitor Count Enable.
#define CR4_PGE      0x00000080u ///< Page Global Enable.
#define CR4_MCE      0x00000040u ///< Machine Check Exceptions.
#define CR4_PAE      0x00000020u ///< Physical Address Extensions.
#define CR4_PSE      0x00000010u ///< Page Size Extensions.
#define CR4_DE       0x00000008u ///< Debugging Extensions.
#define CR4_TSD      0x00000004u ///< Time Stamp Disable.
#define CR4_PVI      0x00000002u ///< Protected-mode Virtual Interrupts.
#define CR4_VME      0x00000001u ///< Virtual-8086 Mode Extensions.

static inline uint16_t get_es(void)
{
    uint16_t es;
    ASM("mov %%es, %0"
        : "=r"(es));
    return es;
}

static inline void set_es(uint16_t es)
{
    ASM("mov %0, %%es"
        :
        : "r"(es));
}

static inline uint16_t get_ds(void)
{
    uint16_t ds;
    ASM("mov %%ds, %0"
        : "=r"(ds));
    return ds;
}

static inline void set_ds(uint16_t ds)
{
    ASM("mov %0, %%ds"
        :
        : "r"(ds));
}

static inline uint16_t get_fs(void)
{
    uint16_t fs;
    ASM("mov %%fs, %0"
        : "=r"(fs));
    return fs;
}

static inline void set_fs(uint16_t fs)
{
    ASM("mov %0, %%fs"
        :
        : "r"(fs));
}

static inline uint16_t get_gs(void)
{
    uint16_t gs;
    ASM("mov %%gs, %0"
        : "=r"(gs));
    return gs;
}

static inline void set_gs(uint16_t gs)
{
    ASM("mov %0, %%gs"
        :
        : "r"(gs));
}

static inline uint16_t get_ss(void)
{
    uint16_t ss;
    ASM("mov %%ss, %0"
        : "=r"(ss));
    return ss;
}

static inline void set_ss(uint16_t ss)
{
    ASM("mov %0, %%ss"
        :
        : "r"(ss));
}

static inline uintptr_t get_cr0(void)
{
    uintptr_t cr0;
    ASM("mov %%cr0, %0"
        : "=r"(cr0));
    return (cr0);
}

static inline void set_cr0(uintptr_t cr0)
{
    ASM("mov %0, %%cr0"
        :
        : "r"(cr0));
}

static inline uintptr_t get_cr3(void)
{
    uintptr_t cr3;
    ASM("mov %%cr3, %0"
        : "=r"(cr3));
    return (cr3);
}

static inline void set_cr3(uintptr_t cr3)
{
    ASM("mov %0, %%cr3"
        :
        : "r"(cr3));
}

static inline uintptr_t get_cr4(void)
{
    uintptr_t cr4;
    ASM("mov %%cr4, %0"
        : "=r"(cr4));
    return (cr4);
}

static inline void set_cr4(uintptr_t cr4)
{
    ASM("mov %0, %%cr4"
        :
        : "r"(cr4)
        : "memory");
}

static inline uintptr_t get_eflags(void)
{
    uintptr_t eflags;
    /* "=rm" is safe here, because "pop" adjusts the stack before
     * it evaluates its effective address -- this is part of the
     * documented behavior of the "pop" instruction.
     */
    ASM("pushf ; pop %0"
        : "=rm"(eflags)
        : /* no input */
        : "memory");
    return eflags;
}

static inline void clear_ts(void)
{
    ASM("clts");
}

static inline unsigned short get_tr(void)
{
    unsigned short seg;
    ASM("str %0"
        : "=rm"(seg));
    return (seg);
}

static inline void set_tr(unsigned int seg)
{
    ASM("ltr %0"
        :
        : "rm"((unsigned short)(seg)));
}

static inline unsigned short sldt(void)
{
    unsigned short seg;
    ASM("sldt %0"
        : "=rm"(seg));
    return (seg);
}

static inline void lldt(unsigned int seg)
{
    ASM("lldt %0"
        :
        : "rm"((unsigned short)(seg)));
}

static inline void lgdt(uintptr_t *desc)
{
    ASM("lgdt %0"
        :
        : "m"(*desc));
}

static inline void lidt(uintptr_t *desc)
{
    ASM("lidt %0"
        :
        : "m"(*desc));
}

/// @brief Enable IRQs.
static inline void sti()
{
    ASM("sti" ::
            : "memory");
}

/// @brief Disable IRQs.
static inline void cli()
{
    ASM("cli" ::
            : "memory");
}

static inline void swapgs(void)
{
    ASM("swapgs");
}

static inline void hlt(void)
{
    ASM("hlt");
}

/// @brief Pause.
static inline void pause()
{
    ASM("pause");
}

// == Memory clobbers =========================================================
// Memory clobber implies a fence, and it also impacts how the compiler treats
// potential data aliases. A memory clobber says that the asm block modifies
// memory that is not otherwise mentioned in the asm instructions.
// So, for example, a correct use of memory clobbers would be when using an
// instruction that clears a cache line. The compiler will assume that
// virtually any data may be aliased with the memory changed by that
// instruction. As a result, all required data used after the asm block
// will be reloaded from memory after the asm completes. This is much more
// expensive than the simple fence implied by the "volatile" attribute.

// == Volatile Block ==========================================================
// Making an inline asm block "volatile" as in this example, ensures that,
// as it optimizes, the compiler does not move any instructions above or
// below the block of asm statements.
//  ASM("  addic. %0,%1,%2\n" : "=r"(res): "=r"(a),"r"(a))
// This can be particularly important in cases when the code is accessing
// shared memory.
