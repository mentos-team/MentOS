/// @file switch.h
/// @brief Low-level kernel-stack context switch primitive.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "kernel.h"
#include "stddef.h"
#include "stdint.h"

/// The assembly epilogue depends on this exact hardware/software frame.
/// Use C99-compatible negative-width typedefs because this project deliberately
/// compiles freestanding C in pedantic C99 mode.
#define SWITCH_LAYOUT_ASSERT(name, condition) typedef char name[(condition) ? 1 : -1]
SWITCH_LAYOUT_ASSERT(switch_pt_regs_size, sizeof(pt_regs_t) == 19U * sizeof(uint32_t));
SWITCH_LAYOUT_ASSERT(switch_pt_regs_gs, __builtin_offsetof(pt_regs_t, gs) == 0U);
SWITCH_LAYOUT_ASSERT(switch_pt_regs_eax, __builtin_offsetof(pt_regs_t, eax) == 11U * sizeof(uint32_t));
SWITCH_LAYOUT_ASSERT(switch_pt_regs_eip, __builtin_offsetof(pt_regs_t, eip) == 14U * sizeof(uint32_t));
SWITCH_LAYOUT_ASSERT(switch_pt_regs_useresp, __builtin_offsetof(pt_regs_t, useresp) == 17U * sizeof(uint32_t));
#undef SWITCH_LAYOUT_ASSERT

/// Bytes occupied by the four callee-saved registers and return address.
#define SWITCH_FRAME_SIZE (5U * sizeof(uint32_t))

/// @brief Switch from one inactive kernel context to another.
///
/// The caller must have interrupts disabled and direction flag clear. The
/// outgoing stack stores, in order, EBP, EDI, ESI, EBX and the return address;
/// the incoming stack must use the same layout. Returning from this function
/// resumes the incoming context at its saved return address.
///
/// @param prev_esp Destination for the outgoing ESP.
/// @param next_esp ESP of the incoming context.
void switch_to(uint32_t *prev_esp, uint32_t next_esp);
