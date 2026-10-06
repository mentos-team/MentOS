/// @file switch.h
/// @brief Low-level kernel-stack context switch primitive.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

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
