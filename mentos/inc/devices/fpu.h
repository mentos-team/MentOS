/// @file fpu.h
/// @brief Floating Point Unit (FPU).
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup devices Hardware Interfaces
/// @brief Data structures and functions for managing hardware devices.
/// @{
/// @addtogroup fpu Floating Point Unit (FPU)
/// @brief Device designed to carry out operations on floating-point numbers.
/// @{
#pragma once

#include "stdint.h"

/// @brief Environment information of floating point unit.
typedef struct env87 {
    /// Control word (16bits).
    long en_cw;
    /// Status word (16bits).
    long en_sw;
    /// Tag word (16bits).
    long en_tw;
    /// Floating point instruction pointer.
    long en_fip;
    /// Floating code segment selector.
    unsigned short en_fcs;
    /// Opcode last executed (11 bits).
    unsigned short en_opcode;
    /// Floating operand offset.
    long en_foo;
    /// Floating operand segment selector.
    long en_fos;
} env87;

/// @brief Contents of each floating point accumulator.
typedef struct fpacc87 {
    /// Easy to access bytes.
    unsigned char fp_bytes[10];
} fpacc87;

/// @brief Floating point context.
typedef struct save87 {
    /// Floating point control/status.
    env87 sv_env;
    /// Accumulator contents, 0-7.
    fpacc87 sv_ac[8];
    /// Padding for (now unused) saved status word.
    unsigned char sv_pad0[4];
    /*
     * Bogus padding for emulators. Emulators should use their own
     * struct and arrange to store into this struct (ending here)
     * before it is inspected for ptracing or for core dumps. Some
     * emulators overwrite the whole struct. We have no good way of
     * knowing how much padding to leave. Leave just enough for the
     * GPL emulator's i387_union (176 bytes total).
     */
    /// Padding used by emulators.
    unsigned char sv_pad[64];
} save87;

/// @brief Stores the XMM environment.
typedef struct envxmm {
    /// Control word (16bits).
    uint16_t en_cw;
    /// Status word (16bits).
    uint16_t en_sw;
    /// Tag word (16bits).
    uint16_t en_tw;
    /// Opcode last executed (11 bits).
    uint16_t en_opcode;
    /// Floating point instruction pointer.
    uint32_t en_fip;
    /// Floating code segment selector.
    uint16_t en_fcs;
    /// Padding.
    uint16_t en_pad0;
    /// Floating operand offset.
    uint32_t en_foo;
    /// Floating operand segment selector.
    uint16_t en_fos;
    /// Padding.
    uint16_t en_pad1;
    /// SSE sontorol/status register.
    uint32_t en_mxcsr;
    /// Valid bits in mxcsr.
    uint32_t en_mxcsr_mask;
} envxmm;

/// @brief Contents of each SSE extended accumulator.
typedef struct xmmacc {
    /// TODO: Comment.
    unsigned char xmm_bytes[16];
} xmmacc;

/// @brief Stores the XMM context.
typedef struct savexmm {
    /// TODO: Comment.
    envxmm sv_env;
    /// TODO: Comment.
    struct {
        /// TODO: Comment.
        fpacc87 fp_acc;
        /// Padding.
        unsigned char fp_pad[6];
    } sv_fp[8];
    /// TODO: Comment.
    xmmacc sv_xmm[8];
    /// Padding.
    unsigned char sv_pad[224];
} __attribute__((__aligned__(16))) savexmm;

/// @brief Stores FPU registers details.
typedef union savefpu {
    /// Stores the floating point context.
    save87 sv_87;
    /// Stores the XMM context.
    savexmm sv_xmm;
} savefpu;

/// @brief Called during a context switch to save the FPU registers status
/// of the currently running thread.
void switch_fpu();

/// @brief Called during a context switch to load the FPU registers status
/// of the currently running thread inside the FPU.
void unswitch_fpu();

/// @brief Enable the FPU context handling.
/// @return 0 if fails, 1 if succeed.
int fpu_install();

/// @}
/// @}
