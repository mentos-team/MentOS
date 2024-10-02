/// @file   proc_access.h
/// @brief  Set of functions and flags used to manage processors registers.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

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

/// @brief Reads the Extra Segment (DS).
/// @return the value we read.
static inline uint16_t get_es(void)
{
    uint16_t es;
    __asm__ __volatile__("mov %%es, %0"
                         : "=r"(es));
    return es;
}

/// @brief Sets the Extra Segment (DS).
/// @param es the value we set.
static inline void set_es(uint16_t es)
{
    __asm__ __volatile__("mov %0, %%es"
                         :
                         : "r"(es));
}

/// @brief Reads the Data Segment (DS).
/// @return the value we read.
static inline uint16_t get_ds(void)
{
    uint16_t ds;
    __asm__ __volatile__("mov %%ds, %0"
                         : "=r"(ds));
    return ds;
}

/// @brief Sets the Data Segment (DS).
/// @param ds the value we set.
static inline void set_ds(uint16_t ds)
{
    __asm__ __volatile__("mov %0, %%ds"
                         :
                         : "r"(ds));
}

/// @brief Reads FS.
/// @return the value we read.
static inline uint16_t get_fs(void)
{
    uint16_t fs;
    __asm__ __volatile__("mov %%fs, %0"
                         : "=r"(fs));
    return fs;
}

/// @brief Sets FS.
/// @param fs the value we set.
static inline void set_fs(uint16_t fs)
{
    __asm__ __volatile__("mov %0, %%fs"
                         :
                         : "r"(fs));
}

/// @brief Reads GS.
/// @return the value we read.
static inline uint16_t get_gs(void)
{
    uint16_t gs;
    __asm__ __volatile__("mov %%gs, %0"
                         : "=r"(gs));
    return gs;
}

/// @brief Sets GS.
/// @param gs the value we set.
static inline void set_gs(uint16_t gs)
{
    __asm__ __volatile__("mov %0, %%gs"
                         :
                         : "r"(gs));
}

/// @brief Reads the Stack Segment (SS).
/// @return the value we read.
static inline uint16_t get_ss(void)
{
    uint16_t ss;
    __asm__ __volatile__("mov %%ss, %0"
                         : "=r"(ss));
    return ss;
}

/// @brief Sets the Stack Segment (SS).
/// @param ss the value we set.
static inline void set_ss(uint16_t ss)
{
    __asm__ __volatile__("mov %0, %%ss"
                         :
                         : "r"(ss));
}

/// @brief Reads the current cr0 value.
/// @return the value we read.
static inline uintptr_t get_cr0(void)
{
    uintptr_t cr0;
    __asm__ __volatile__("mov %%cr0, %0"
                         : "=r"(cr0));
    return (cr0);
}

/// @brief Sets the cr0 value.
/// @param cr0 the value we want to set.
static inline void set_cr0(uintptr_t cr0)
{
    __asm__ __volatile__("mov %0, %%cr0"
                         :
                         : "r"(cr0));
}


/// @brief Reads the current cr2 value.
/// @return the value we read.
static inline uintptr_t get_cr2(void)
{
    uintptr_t cr2;
    __asm__ __volatile__("mov %%cr2, %0"
                         : "=r"(cr2));
    return (cr2);
}

/// @brief Sets the cr2 value.
/// @param cr2 the value we want to set.
static inline void set_cr2(uintptr_t cr2)
{
    __asm__ __volatile__("mov %0, %%cr2"
                         :
                         : "r"(cr2));
}

/// @brief Reads the current cr3 value.
/// @return the value we read.
static inline uintptr_t get_cr3(void)
{
    uintptr_t cr3;
    __asm__ __volatile__("mov %%cr3, %0"
                         : "=r"(cr3));
    return (cr3);
}

/// @brief Sets the cr3 value.
/// @param cr3 the value we want to set.
static inline void set_cr3(uintptr_t cr3)
{
    __asm__ __volatile__("mov %0, %%cr3"
                         :
                         : "r"(cr3));
}

/// @brief Reads the current cr4 value.
/// @return the value we read.
static inline uintptr_t get_cr4(void)
{
    uintptr_t cr4;
    __asm__ __volatile__("mov %%cr4, %0"
                         : "=r"(cr4));
    return (cr4);
}

/// @brief Sets the cr4 value.
/// @param cr4 the value we want to set.
static inline void set_cr4(uintptr_t cr4)
{
    __asm__ __volatile__("mov %0, %%cr4"
                         :
                         : "r"(cr4)
                         : "memory");
}

/// @brief Reads entire contents of the EFLAGS register.
/// @return the content of EFLAGS.
static inline uintptr_t get_eflags(void)
{
    uintptr_t eflags;
    // "=rm" is safe here, because "pop" adjusts the stack before
    // it evaluates its effective address -- this is part of the
    // documented behavior of the "pop" instruction.
    __asm__ __volatile__("pushf; pop %0"
                         : "=rm"(eflags)
                         : // no input
                         : "memory");
    return eflags;
}

/// @brief Clears the task-switched (TS) flag in the CR0 register.
static inline void clear_ts(void)
{
    __asm__ __volatile__("clts");
}

/// @brief Reads the segment selector from the task register (TR).
/// @return
static inline unsigned short get_tr(void)
{
    unsigned short seg;
    __asm__ __volatile__("str %0"
                         : "=rm"(seg));
    return (seg);
}

/// @brief Loads the source operand into the segment selector field of the task register.
/// @param seg the segment selector we want to set.
static inline void set_tr(unsigned short seg)
{
    __asm__ __volatile__("ltr %0"
                         :
                         : "rm"(seg));
}

/// @brief Reads the segment selector from the local descriptor table register (LDTR).
/// @return the segment selector.
static inline unsigned short sldt(void)
{
    unsigned short seg;
    __asm__ __volatile__("sldt %0"
                         : "=rm"(seg));
    return (seg);
}

/// @brief Loads the source operand into the segment selector field of the local descriptor table register (LDTR).
/// @param seg The segment selector we need to set.
static inline void lldt(unsigned short seg)
{
    __asm__ __volatile__("lldt %0"
                         :
                         : "rm"(seg));
}

/// @brief Loads the values in the source operand into the global descriptor
/// table register (GDTR) or the interrupt descriptor table register (IDTR).
/// @param desc the value we need to load.
static inline void lgdt(uintptr_t *desc)
{
    __asm__ __volatile__("lgdt %0"
                         :
                         : "m"(*desc));
}

/// @brief Loads the values in the source operand into the global descriptor
/// table register (GDTR) or the interrupt descriptor table register (IDTR).
/// @param desc the value we need to load.
static inline void lidt(uintptr_t *desc)
{
    __asm__ __volatile__("lidt %0"
                         :
                         : "m"(*desc));
}

/// @brief Set interrupt flag; external, maskable interrupts enabled at the end
/// of the next instruction.
static inline void sti(void)
{
    __asm__ __volatile__("sti"
                         :
                         :
                         : "memory");
}

/// @brief Clear interrupt flag; interrupts disabled when interrupt flag
/// cleared.
static inline void cli(void)
{
    __asm__ __volatile__("cli"
                         :
                         :
                         : "memory");
}

/// @brief Exchanges the current GS base register value with the value contained
/// in MSR address C0000102H.
static inline void swapgs(void)
{
    __asm__ __volatile__("swapgs");
}

/// @brief Halts the CPU until the next external interrupt is fired.
static inline void hlt(void)
{
    __asm__ __volatile__("hlt");
}

/// @brief Gives hint to processor that improves performance of spin-wait loops.
static inline void pause(void)
{
    __asm__ __volatile__("pause");
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
//  __asm__ __volatile__("  addic. %0,%1,%2\n" : "=r"(res): "=r"(a),"r"(a))
// This can be particularly important in cases when the code is accessing
// shared memory.
