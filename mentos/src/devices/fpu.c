/// @file fpu.c
/// @brief Floating Point Unit (FPU).
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[FPU   ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "devices/fpu.h"
#include "descriptor_tables/isr.h"
#include "io/debug.h"
#include "string.h"
#include "assert.h"
#include "process/scheduler.h"
#include "math.h"
#include "process/process.h"
#include "system/signal.h"

/// Pointerst to the current thread using the FPU.
task_struct *thread_using_fpu = NULL;
/// Temporary aligned buffer for copying around FPU contexts.
uint8_t saves[512] __attribute__((aligned(16)));

/// @brief Set the FPU control word.
/// @param cw What to set the control word to.
static inline void __set_fpu_cw(const uint16_t cw)
{
    __asm__ __volatile__("fldcw %0" ::"m"(cw));
}

/// @brief Enable the FPU and SSE.
static inline void __enable_fpu()
{
    __asm__ __volatile__("clts");
    size_t t;
    __asm__ __volatile__("mov %%cr0, %0"
                 : "=r"(t));
    t &= ~(1U << 2U);
    t |= (1U << 1U);
    __asm__ __volatile__("mov %0, %%cr0" ::"r"(t));
    __asm__ __volatile__("mov %%cr4, %0"
                 : "=r"(t));
    t |= 3U << 9U;
    __asm__ __volatile__("mov %0, %%cr4" ::"r"(t));
}

/// Disable FPU and SSE so it traps to the kernel.
static inline void __disable_fpu()
{
    size_t t;

    __asm__ __volatile__("mov %%cr0, %0"
                 : "=r"(t));

    t |= 1U << 3U;

    __asm__ __volatile__("mov %0, %%cr0" ::"r"(t));
}

/// @brief Restore the FPU for a process.
static inline void __restore_fpu(task_struct *proc)
{
    assert(proc && "Trying to restore FPU of NULL process.");

    memcpy(&saves, (uint8_t *)&proc->thread.fpu_register, 512);

    __asm__ __volatile__("fxrstor (%0)" ::"r"(saves));
}

/// Save the FPU for a process.
static inline void __save_fpu(task_struct *proc)
{
    assert(proc && "Trying to save FPU of NULL process.");

    __asm__ __volatile__("fxsave (%0)" ::"r"(saves));

    memcpy((uint8_t *)&proc->thread.fpu_register, &saves, 512);
}

/// Initialize the FPU.
static inline void __init_fpu()
{
    __asm__ __volatile__("fninit");
}

/// Kernel trap for FPU usage when FPU is disabled.
/// @param f The interrupt stack frame.
static inline void __invalid_op(pt_regs *f)
{
    pr_debug("__invalid_op(%p)\n", f);
    // First, turn the FPU on.
    __enable_fpu();
    if (thread_using_fpu == scheduler_get_current_process()) {
        // If this is the thread that last used the FPU, do nothing.
        return;
    }
    if (thread_using_fpu) {
        // If there is a thread that was using the FPU, save its state.
        __save_fpu(thread_using_fpu);
    }
    thread_using_fpu = scheduler_get_current_process();
    if (!thread_using_fpu->thread.fpu_enabled) {
        /*
         * If the FPU has not been used in this thread previously,
         * we need to initialize it.
         */
        __init_fpu();
        thread_using_fpu->thread.fpu_enabled = true;
        return;
    }
    // Otherwise we restore the context for this thread.
    __restore_fpu(thread_using_fpu);
}

/// Kernel trap for various integer and floating-point errors
/// @param f The interrupt stack frame.
static inline void __sigfpe_handler(pt_regs* f) 
{
    pr_debug("__sigfpe_handler(%p)\n", f);

    // Notifies current process
    thread_using_fpu = scheduler_get_current_process();
    sys_kill(thread_using_fpu->pid, SIGFPE);
}


/// @brief Ensure basic FPU functionality works.
/// @details
/// For processors without a FPU, this tests that maths libraries link
/// correctly.
static int __fpu_test()
{
    double a = M_PI;
    // First test.
    for (int i = 0; i < 10000; i++) {
        a = a * 1.123 + (a / 3);
        a /= 1.111;
        while (a > 100.0)
            a /= 3.1234563212;
        while (a < 2.0)
            a += 1.1232132131;
    }
    if (a != 50.11095685350556294679336133413)
        return 0;
    // Second test.
    a = M_PI;
    for (int i = 0; i < 100; i++)
        a = a * 3 + (a / 3);
    return (a == 60957114488184560000000000000000000000000000000000000.0);
}

void switch_fpu()
{
    __save_fpu(scheduler_get_current_process());
}

void unswitch_fpu()
{
    __restore_fpu(scheduler_get_current_process());
}

int fpu_install()
{
    __enable_fpu();
    __init_fpu();
    __save_fpu(scheduler_get_current_process());

    // Install the handler for device missing
    isr_install_handler(DEV_NOT_AVL, &__invalid_op, "fpu: device missing");

    // Install handlers for floating points and integers errors
    isr_install_handler(DIVIDE_ERROR, &__sigfpe_handler, "divide error");

    // NB: The exceptions bolow don't seems to ever trigger
    //isr_install_handler(OVERFLOW,           &__sigfpe_handler, "overflow");
    //isr_install_handler(FLOATING_POINT_ERR, &__sigfpe_handler, "floating point error");

    return __fpu_test();
}
