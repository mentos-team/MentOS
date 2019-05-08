///                MentOS, The Mentoring Operating system project
/// @file timer.c
/// @brief Timer implementation.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "timer.h"
#include "list.h"
#include "kheap.h"
#include "debug.h"
#include "stdint.h"
#include "pic8259.h"
#include "port_io.h"
#include "irqflags.h"

#define SUBTICKS_PER_TICK 1000

/// This will keep track of how many ticks the system has been running for.
static __volatile__ uint64_t timer_subticks = 0;
/// This will keep track of how many seconds the system has been running for.
static __volatile__ uint64_t timer_ticks = 0;

/// The list of wakeup functions.
// static list_t *wakeup_list;

void timer_phase(const uint32_t hz)
{
	// Calculate our divisor.
	uint32_t divisor = PIT_DIVISOR / hz;
	// Set our command byte 0x36.
	outportb(PIT_COMREG, PIT_CONFIGURATION);
	// Set low byte of divisor.
	outportb(PIT_DATAREG0, divisor & PIT_MASK);
	// Set high byte of divisor.
	outportb(PIT_DATAREG0, (divisor >> 8) & PIT_MASK);
}

void timer_handler(pt_regs *reg)
{
	(void)reg;
	// Check if a second has passed.
	if ((++timer_subticks % SUBTICKS_PER_TICK) == 0) {
		++timer_ticks;
	}
	/*
     * if (!list_empty(wakeup_list))
     * {
     *     listnode_foreach(t, wakeup_list)
     *     {
     *          wakeup_info_t *wakeupInfo = (wakeup_info_t *) t->value;
     *          if (timer_subticks >= wakeupInfo->wakeup_at_jiffy)
     *          {
     *              // Call the function.
     *              wakeupInfo->func();
     *              // Reset the timer.
     *              wakeupInfo->wakeup_at_jiffy =
     *              timer_subticks + wakeupInfo->period * timer_hz;
     *          }
     *     {
     *  }
     */

	kernel_schedule(reg);
	// The ack is sent to PIC only when all handlers terminated!
	pic8259_send_eoi(IRQ_TIMER);
}

void timer_install()
{
	// Intialize the wakeup list (before installing the timer).
	// wakeup_list = list_create();
	// Set the timer phase.
	timer_phase(SUBTICKS_PER_TICK);
	// Installs 'timer_handler' to IRQ0.
	irq_install_handler(IRQ_TIMER, timer_handler, "timer");
	// Enable the IRQ of the itemer.
	pic8259_irq_enable(IRQ_TIMER);
}

uint64_t timer_get_ticks()
{
	return timer_ticks;
}

uint64_t timer_get_subticks()
{
	return timer_subticks;
}

/*
 * void timer_register_wakeup_call(wakeup_callback_t func, uint32_t period)
 * {
 *
 *     // Save the function sec, and jiffy to a list, when timer hits that
 *     // func's jiffy it will call the func, and update next jiffies.
 *
 *     wakeup_info_t *wakeupInfo = kmalloc(sizeof(wakeup_info_t));
 *
 *     wakeupInfo->func = func;
 *
 *     wakeupInfo->wakeup_at_jiffy = timer_subticks + period * timer_hz;
 *
 *     wakeupInfo->period = period;
 *
 *     list_insert_front(wakeup_list, wakeupInfo);
 * }
 */

void sleep(unsigned int seconds)
{
	uint32_t future_time = timer_ticks + seconds;

	while (timer_ticks <= future_time)
		;
}
