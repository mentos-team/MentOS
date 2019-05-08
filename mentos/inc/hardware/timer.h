///                MentOS, The Mentoring Operating system project
/// @file timer.h
/// @brief Programmable Interval Timer (PIT) definitions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "kernel.h"
#include "stdint.h"

/// @defgroup picregs Programmable Interval Timer Registers
/// @brief The list of registers used to set the PIT.
/// @{

/// Channel 0 data port (read/write).
#define PIT_DATAREG0 0x40

/// Channel 1 data port (read/write).
#define PIT_DATAREG1 0x41

/// Channel 2 data port (read/write).
#define PIT_DATAREG2 0x42

/// Mode/Command register (write only, a read is ignored).
#define PIT_COMREG 0x43

/// @}

/// @brief Frequency divider value (1.193182 MHz).
#define PIT_DIVISOR 1193180

/// @brief   Command used to configure the PIT.
/// @details
///          0x36 = 00110110B.
///          Channel         |  00 | Select Channel 0.
///          Access mode     |  11 | lobyte/hibyte.
///          Operating mode  | 011 | Mode 3 (square wave generator).
///          BCD/Binary mode |   0 | 16-bit binary.
#define PIT_CONFIGURATION 0x36

#define PIT_MASK 0xFF

/// @brief Pointer to a functionality to wake up.
typedef void (*wakeup_callback_t)();

/// @brief Holds the information about a wake-up functionality.
typedef struct wakeup_info {
	/// Pointer to the functionality.
	wakeup_callback_t func;
	/// The tick, in the future, when the functionality must be triggered.
	__volatile__ uint32_t wakeup_at_jiffy;
	/// The period in seconds.
	uint32_t period;
} wakeup_info_t;

/// @brief   Handles the timer.
/// @details In this case, it's very simple: We
///          increment the 'timer_ticks' variable every time the
///          timer fires. By default, the timer fires 18.222 times
///          per second. Why 18.222Hz? Some engineer at IBM must've
///          been smoking something funky
void timer_handler(pt_regs *reg);

/// @brief Sets up the system clock by installing the timer handler into IRQ0.
void timer_install();

/// @brief Returns the number of ticks since the system is running.
uint64_t timer_get_ticks();

/// @brief Returns the number of ticks since the system is running.
uint64_t timer_get_subticks();

/// @brief         Registers a function which will be waken up at each tick.
/// @param func   The functionality which must be triggered.
/// @param period The period in second.
void timer_register_wakeup_call(wakeup_callback_t func, uint32_t period);

/// @brief         Makes the process sleep for the given ammount of time.
/// @param seconds The ammount of seconds.
void sleep(unsigned int seconds);

/// @brief Allows to set the timer phase to the given frequency.
/// @param hz The frequency to set.
void timer_phase(const uint32_t hz);
