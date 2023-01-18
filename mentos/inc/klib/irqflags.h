/// @file irqflags.h
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "proc_access.h"
#include "stddef.h"
#include "stdint.h"

/// @brief   Enable IRQs (nested).
/// @details If called after calling irq_disable, this function will not
/// activate IRQs if they were not active before.
/// @param flags the flags to control this behaviour.
inline static void irq_enable(uint8_t flags)
{
    if (flags) {
        sti();
    }
}

/// @brief   Disable IRQs (nested).
/// @details Disable IRQs when unsure if IRQs were enabled at all. This function
/// together with irq_enable can be used in situations when interrupts
/// shouldn't be activated if they were not activated before calling this
/// function.
/// @return 1 if the IRQ is enable for the CPU.
inline static uint8_t irq_disable()
{
    size_t flags;
    // We are pushing the entire contents of the EFLAGS register onto the stack,
    // clearing the interrupt line, and with the pop, getting the current status
    // of the flags.
    __asm__ __volatile__("pushf; cli; pop %0;"
                         : "=r"(flags)
                         :
                         : "memory");
    return flags & (1 << 9);
}

/// @brief Determines, if the interrupt flags (IF) is set.
/// @return 1 if the IRQ is enable for the CPU.
inline static uint8_t is_irq_enabled()
{
    size_t flags;
    __asm__ __volatile__("pushf; pop %0;"
                         : "=r"(flags)
                         :
                         : "memory");
    return flags & (1 << 9);
}
