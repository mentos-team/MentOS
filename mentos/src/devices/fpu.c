///                MentOS, The Mentoring Operating system project
/// @file fpu.c
/// @brief Floating Point Unit (FPU).
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fpu.h"
#include "isr.h"
#include "debug.h"
#include "string.h"
#include "assert.h"
#include "process.h"
#include "scheduler.h"

#define NO_LAZY_FPU

struct task_struct *fpu_thread = NULL;

/// @brief Set the FPU control word.
/// @param cw What to set the control word to.
void set_fpu_cw(const uint16_t cw)
{
	asm volatile("fldcw %0" ::"m"(cw));
}

/// @brief Enable the FPU and SSE.
void enable_fpu()
{
	asm volatile("clts");

	size_t t;

	asm volatile("mov %%cr0, %0" : "=r"(t));

	t &= ~(1 << 2);

	t |= (1 << 1);

	asm volatile("mov %0, %%cr0" ::"r"(t));

	asm volatile("mov %%cr4, %0" : "=r"(t));

	t |= 3 << 9;

	asm volatile("mov %0, %%cr4" ::"r"(t));
}

/// Disable FPU and SSE so it traps to the kernel.
void disable_fpu()
{
	size_t t;

	asm volatile("mov %%cr0, %0" : "=r"(t));

	t |= 1 << 3;

	asm volatile("mov %0, %%cr0" ::"r"(t));
}

// Temporary aligned buffer for copying around FPU contexts.
uint8_t saves[512] __attribute__((aligned(16)));

/// @brief Restore the FPU for a process.
void restore_fpu(struct task_struct *proc)
{
	assert(proc && "Trying to restore FPU of NULL process.");

	memcpy(&saves, (uint8_t *)&proc->thread.fpu_register, 512);

	asm volatile("fxrstor (%0)" ::"r"(saves));
}

/// Save the FPU for a process.
void save_fpu(struct task_struct *proc)
{
	assert(proc && "Trying to save FPU of NULL process.");

	asm volatile("fxsave (%0)" ::"r"(saves));

	memcpy((uint8_t *)&proc->thread.fpu_register, &saves, 512);
}

/// Initialize the FPU.
void init_fpu()
{
	asm volatile("fninit");
}

/// Kernel trap for FPU usage when FPU is disabled.
void invalid_op(pt_regs *r)
{
	// First, turn the FPU on.
	enable_fpu();
	if (fpu_thread == kernel_get_current_process()) {
		// If this is the thread that last used the FPU, do nothing.
		return;
	}
	if (fpu_thread) {
		// If there is a thread that was using the FPU, save its state.
		save_fpu(fpu_thread);
	}
	fpu_thread = kernel_get_current_process();
	if (!fpu_thread->thread.fpu_enabled) {
		/*
         * If the FPU has not been used in this thread previously,
         * we need to initialize it.
         */
		init_fpu();
		fpu_thread->thread.fpu_enabled = true;
		return;
	}
	// Otherwise we restore the context for this thread.
	restore_fpu(fpu_thread);
}

// Called during a context switch; disable the FPU.
void switch_fpu()
{
#ifdef NO_LAZY_FPU
	save_fpu(kernel_get_current_process());
#else
	disable_fpu();
#endif
}

void unswitch_fpu()
{
#ifdef NO_LAZY_FPU
	restore_fpu(kernel_get_current_process());
#endif
}

static bool_t fpu_test_1()
{
	double a = M_PI;
	for (int i = 0; i < 10000; i++) {
		a = a * 1.123 + (a / 3);
		a /= 1.111;
		while (a > 100.0) {
			a /= 3.1234563212;
		}
		while (a < 2.0) {
			a += 1.1232132131;
		}
	}
	return (a == 50.11095685350556294679336133413);
}

/*
 * Ensure basic FPU functionality works.
 *
 * For processors without a FPU, this tests that maths libraries link
 * correctly.
 */
static bool_t fpu_test_2()
{
	double a = M_PI;
	for (int i = 0; i < 100; i++) {
		a = a * 3 + (a / 3);
	}
	return (a == 60957114488184560000000000000000000000000000000000000.0);
}

// Enable the FPU context handling.
bool_t fpu_install()
{
#ifdef NO_LAZY_FPU
	enable_fpu();
	init_fpu();
	save_fpu(kernel_get_current_process());
#else
	enable_fpu();
	disable_fpu();
#endif
	isr_install_handler(7, &invalid_op, "fpu");
	return fpu_test_1() & fpu_test_2();
}
