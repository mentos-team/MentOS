/// @file timer.c
/// @brief Timer implementation.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[TIMER ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "assert.h"
#include "descriptor_tables/isr.h"
#include "devices/fpu.h"
#include "drivers/rtc.h"
#include "hardware/pic8259.h"
#include "hardware/timer.h"
#include "io/port_io.h"
#include "io/video.h"
#include "klib/irqflags.h"
#include "mem/kheap.h"
#include "process/scheduler.h"
#include "process/wait.h"
#include "stdint.h"
#include "sys/errno.h"
#include "system/signal.h"
#include "system/panic.h"
#include "string.h"

/// @defgroup picregs Programmable Interval Timer Registers
/// @brief The list of registers used to set the PIT.
/// @{

/// Channel 0 data port (read/write).
#define PIT_DATAREG0 0x40u

/// Channel 1 data port (read/write).
#define PIT_DATAREG1 0x41u

/// Channel 2 data port (read/write).
#define PIT_DATAREG2 0x42u

/// Mode/Command register (write only, a read is ignored).
#define PIT_COMREG 0x43u

/// @}

/// @brief Frequency divider value (1.193182 MHz).
#define PIT_DIVISOR 1193182

/// @brief   Command used to configure the PIT.
/// @details
/// Bits         Usage
/// 6 and 7 [Select channel]
///     0 0 = Channel 0
///     0 1 = Channel 1
///     1 0 = Channel 2
///     1 1 = Read-back command (8254 only)
/// 4 and 5 [Access mode]
///     0 0 = Latch count value command
///     0 1 = Access mode: lobyte only
///     1 0 = Access mode: hibyte only
///     1 1 = Access mode: lobyte/hibyte
/// 1 to 3  [Operating mode]
///     0 0 0 = Mode 0 (interrupt on terminal count)
///     0 0 1 = Mode 1 (hardware re-triggerable one-shot)
///     0 1 0 = Mode 2 (rate generator)
///     0 1 1 = Mode 3 (square wave generator)
///     1 0 0 = Mode 4 (software triggered strobe)
///     1 0 1 = Mode 5 (hardware triggered strobe)
///     1 1 0 = Mode 2 (rate generator, same as 010b)
///     1 1 1 = Mode 3 (square wave generator, same as 011b)
/// 0       [BCD/Binary mode]
///     0 = 16-bit binary
///     1 = four-digit BCD
///
/// Examples:
///     0x36 = 00|11|011|0
///     0x34 = 00|11|010|0
#define PIT_CONFIGURATION 0x34u

/// Mask used to set the divisor.
#define PIT_MASK 0xFFu

/// The number of ticks since the system started its execution.
static __volatile__ unsigned long timer_ticks = 0;

void timer_phase(const uint32_t hz)
{
    // Calculate our divisor.
    unsigned int divisor = PIT_DIVISOR / hz;
    // Set our command byte 0x36.
    outportb(PIT_COMREG, PIT_CONFIGURATION);
    // Set low byte of divisor.
    outportb(PIT_DATAREG0, divisor & PIT_MASK);
    // Set high byte of divisor.
    outportb(PIT_DATAREG0, (divisor >> 8u) & PIT_MASK);
}

void timer_handler(pt_regs *reg)
{
    // Save current process fpu state.
    switch_fpu();
    // Check if a second has passed.
    ++timer_ticks;
    // Update all timers
    run_timer_softirq();
    // Perform the schedule.
    scheduler_run(reg);
    // Update graphics.
    video_update();
    // Restore fpu state.
    unswitch_fpu();
    // The ack is sent to PIC only when all handlers terminated!
    pic8259_send_eoi(IRQ_TIMER);
}

void timer_install(void)
{
    dynamic_timers_install();

    // Set the timer phase.
    timer_phase(TICKS_PER_SECOND);
    // Installs 'timer_handler' to IRQ0.
    irq_install_handler(IRQ_TIMER, timer_handler, "timer");
    // Enable the IRQ of the timer.
    pic8259_irq_enable(IRQ_TIMER);
}

uint64_t timer_get_seconds(void)
{
    return timer_ticks / TICKS_PER_SECOND;
}

unsigned long timer_get_ticks(void)
{
    return timer_ticks;
}

// ============================================================================
// SUPPORT STRUCTURES
// ============================================================================

/// @brief Contains the entry of a wait queue and timespec which keeps trakc of
///        the remaining time.
typedef struct sleep_data_t {
    /// POinter to the entry of a wait queue.
    wait_queue_entry_t *wait_queue_entry;
    /// Keeps track of the remaining time.
    timespec *remaining;
} sleep_data_t;

// ============================================================================
// SUPPORT FUNCTIONS (tvec_base_t)
// ============================================================================

static inline void __print_vector(list_head *vector)
{
#if defined(ENABLE_REAL_TIMER_SYSTEM_DUMP) && (__DEBUG_LEVEL__ == LOGLEVEL_DEBUG)
    if (!list_head_empty(vector)) {
        pr_debug("0x%p = [ ", vector);
        list_for_each_decl(it, vector)
        {
            pr_debug("0x%p ", it);
        }
        pr_debug("]\n");
    }
#endif
}

static inline void __print_vector_base(tvec_base_t *base)
{
#if defined(ENABLE_REAL_TIMER_SYSTEM_DUMP) && (__DEBUG_LEVEL__ == LOGLEVEL_DEBUG)
    pr_debug("========================================\n");
    for (int i = 0; i < TVR_SIZE; ++i) {
        if (!list_head_empty(&base->tvr[i])) {
            pr_debug("- TVR[%u] -----------------------------\n", i);
            __print_vector(&base->tvr[i]);
        }
    }
    for (int j = 0; j < TVN_COUNT; ++j) {
        for (int i = 0; i < TVN_SIZE; ++i) {
            if (!list_head_empty(&base->tvn[j][i])) {
                pr_debug("- TVN[%u][%2u] -------------------------\n", j, i);
                __print_vector(&base->tvn[j][i]);
            }
        }
    }
    pr_debug("========================================\n");
#endif
}

/// Prints used slots of timer vector
static void __print_vector_slots(list_head *vector, size_t size)
{
#if defined(ENABLE_REAL_TIMER_SYSTEM_DUMP) && (__DEBUG_LEVEL__ == LOGLEVEL_DEBUG)
    char str[TVR_SIZE + 1] = { 0 };
    for (size_t i = 0; i < size; ++i) {
        // New line in order to not clutter the screen
        if ((i != 0) && !(i % TVN_SIZE)) {
            pr_debug("%s", str);
        }
        if (list_head_empty(vector + i)) {
            str[i] = '0';
        } else {
            str[i] = '1';
        }
    }
    pr_debug("%s\n", str);
#endif
}

/// Dump all timer vector in base
static inline void __dump_all_tvec_slots(tvec_base_t *base)
{
#if defined(ENABLE_REAL_TIMER_SYSTEM_DUMP) && (__DEBUG_LEVEL__ == LOGLEVEL_DEBUG)
    __print_vector_slots(base->tvr, TVR_SIZE);
    __print_vector_slots(base->tvn[0], TVN_SIZE);
    __print_vector_slots(base->tvn[1], TVN_SIZE);
    __print_vector_slots(base->tvn[2], TVN_SIZE);
    __print_vector_slots(base->tvn[3], TVN_SIZE);
#endif
}

static inline void __tvec_base_init(tvec_base_t *base)
{
    spinlock_init(&base->lock);
    base->running_timer = NULL;
#ifdef ENABLE_REAL_TIMER_SYSTEM
    base->timer_ticks = timer_get_ticks();
    for (int i = 0; i < TVR_SIZE; ++i) {
        list_head_init(&base->tvr[i]);
    }
    for (int j = 0; j < TVN_COUNT; ++j) {
        for (int i = 0; i < TVN_SIZE; ++i) {
            list_head_init(&base->tvn[j][i]);
        }
    }
#else
    list_head_init(&base.list);
#endif
}

// ============================================================================
// SUPPORT FUNCTIONS (timer_list, sleep_data)
// ============================================================================

/// @brief Allocates the memory for timer.
/// @return a pointer to the allocated timer.
static inline struct timer_list *__timer_list_alloc()
{
    // Allocate the memory.
    struct timer_list *timer = (struct timer_list *)kmalloc(sizeof(struct timer_list));
    pr_debug("ALLOCATE TIMER 0x%p (0x%p)\n", timer, &timer->entry);
    // Check the allocated memory.
    assert(timer && "Failed to allocate memory for a timer.");
    // Clean the memory.
    memset(timer, 0, sizeof(struct timer_list));
    // Initialize the timer.
    spinlock_init(&timer->lock);
    list_head_init(&timer->entry);
    timer->expires  = 0;
    timer->function = NULL;
    timer->data     = 0;
    timer->base     = NULL;
    // Return the timer.
    return timer;
}

/// @brief Frees the memory of a timer.
/// @param timer pointer to the timer.
static inline void __timer_list_dealloc(struct timer_list *timer)
{
    assert(timer && "Received a NULL pointer.");
    pr_debug("FREE TIMER     0x%p (0x%p)\n", timer, &timer->entry);
    // Remove the timer.
    remove_timer(timer);
    // Deallocate the timer memory.
    kfree(timer);
}

/// @brief Allocates the memory for sleep_data.
/// @return a pointer to the allocated sleep_data.
static inline struct sleep_data_t *__sleep_data_alloc()
{
    // Allocate the memory.
    sleep_data_t *sleep_data = (sleep_data_t *)kmalloc(sizeof(sleep_data_t));
    pr_debug("ALLOCATE SLEEP_DATA 0x%p\n", sleep_data);
    // Check the allocated memory.
    assert(sleep_data && "Failed to allocate memory for a sleep_data.");
    // Clean the memory.
    memset(sleep_data, 0, sizeof(sleep_data_t));
    // Initialize the sleep_data.
    sleep_data->wait_queue_entry = NULL;
    sleep_data->remaining        = NULL;
    // Return the sleep_data.
    return sleep_data;
}

/// @brief Frees the memory of a sleep_data.
/// @param sleep_data pointer to the sleep_data.
static inline void __sleep_data_dealloc(sleep_data_t *sleep_data)
{
    assert(sleep_data && "Received a NULL pointer.");
    pr_debug("FREE     SLEEP_DATA 0x%p\n", sleep_data);
    // Deallocate the sleep_data memory.
    kfree(sleep_data);
}

//======================================================================================
// Dynamics timers

/// Contains timer for each CPU (for now only one)
static tvec_base_t cpu_base = { 0 };
/// Contains all process waiting for a sleep.
static wait_queue_head_t sleep_queue;

/// @brief Initialize dynamic timer system
void dynamic_timers_install(void)
{
    // Initialize the timer structure for each CPU.
    __tvec_base_init(&cpu_base);
    // Initialize sleeping process list
    list_head_init(&sleep_queue.task_list);
    spinlock_init(&sleep_queue.lock);
}

/// Select correct timer vector and position inside of it for the input timer
/// index contains the position inside of the tv_index timer vector
static list_head *__timer_get_vector(tvec_base_t *base, struct timer_list *timer)
{
    time_t expires = timer->expires;
    long ticks     = expires - base->timer_ticks;
    // Can happen if you add a timer with expires == ticks, or in the past
    if (ticks < 0) {
        return base->tvr + (base->timer_ticks & TVR_MASK);
    }
    if (ticks < TIMER_TICKS(0)) {
        return base->tvr + (expires & TVR_MASK);
    }
    if (ticks < TIMER_TICKS(1)) {
        return base->tvn[0] + ((expires >> TIMER_TICKS_BITS(0)) & TVN_MASK);
    }
    if (ticks < TIMER_TICKS(2)) {
        return base->tvn[1] + ((expires >> TIMER_TICKS_BITS(1)) & TVN_MASK);
    }
    if (ticks < TIMER_TICKS(3)) {
        return base->tvn[2] + ((expires >> TIMER_TICKS_BITS(2)) & TVN_MASK);
    }
    return base->tvn[3] + ((expires >> TIMER_TICKS_BITS(3)) & TVN_MASK);
}

/// Move all timers from tv up one level
static void __timer_cascate(tvec_base_t *base, size_t vector_list_index, size_t vector_index)
{
    list_head *current_vector, *vector;
    struct timer_list *timer;
    // Get the current vector.
    current_vector = &base->tvn[vector_list_index][vector_index];
    // Migrate only if the vector actually has a timer in it.
    if (!list_head_empty(current_vector)) {
        pr_debug("Migrate from vector 0x%p\n", current_vector);
        // Reinsert all timers into base in the new correct list.
        list_for_each_safe_decl(it, save, current_vector)
        {
            // Get the timer.
            timer = list_entry(it, struct timer_list, entry);
            // Get the new vector.
            vector = __timer_get_vector(base, timer);
            // If the new vector is different than the current one, move the timer.
            if (vector != current_vector) {
                assert(!list_head_empty(it));

                // First, remove the timer from the current list.
                list_head_remove(it);
                // Then, re-initialize the timer.
                init_timer(timer);
                // Insert the timer inside the vector.
                list_head_insert_before(it, vector);
                // Since we are moving timers around, print the vector base.
                pr_debug("Migrate timer (0x%p) 0x%p -> 0x%p\n", it, current_vector, vector);
                __print_vector_base(base);
            }
        }
    }
}

void run_timer_softirq(void)
{
    struct timer_list *timer;
    tvec_base_t *base = &cpu_base;
    spinlock_lock(&base->lock);

#ifdef ENABLE_REAL_TIMER_SYSTEM

    // While we are not up to date with current ticks
    while (base->timer_ticks <= timer_get_ticks()) {
        // Index of the current timer to execute.
        uint32_t current_time_index = base->timer_ticks & TVR_MASK;

        // If the index is zero then all lists in base->tvr have been checked, so they are empty
        if (!current_time_index) {
            // Consider the first invocation of the cascade() function: it receives as arguments
            // the address in base, the address of base->tv2, and the index of the list
            // in base->tv2 including the timers that will decay in the next 256 ticks. This
            // index is determined by looking at the proper bits of the base->timer_ticks value.
            // cascade() moves all dynamic timers in the base->tv2 list into the
            // proper lists of base->tvr; then, it returns a positive value, unless all base->tv2
            // lists are now empty. If so, cascade() is invoked once more to replenish
            // base->tv2 with the timers included in a list of base->tv3, and so on.

            __timer_cascate(base, 3, (base->timer_ticks >> TIMER_TICKS_BITS(3)) & TVN_MASK);
            __timer_cascate(base, 2, (base->timer_ticks >> TIMER_TICKS_BITS(2)) & TVN_MASK);
            __timer_cascate(base, 1, (base->timer_ticks >> TIMER_TICKS_BITS(1)) & TVN_MASK);
            __timer_cascate(base, 0, (base->timer_ticks >> TIMER_TICKS_BITS(0)) & TVN_MASK);
        }

        // If there are timers to execute in this instant
        if (!list_head_empty(&base->tvr[current_time_index])) {
            // Trigger all timers
            list_for_each_safe_decl(it, store, &base->tvr[current_time_index])
            {
                // Get the timer.
                timer = list_entry(it, struct timer_list, entry);
                pr_debug("Execute timer (0x%p)\n", it);
                spinlock_unlock(&base->lock);
                // Executes the timer function.
                if (timer->function) {
                    timer->function(timer->data);
                } else {
                    pr_alert("Dynamic timer function is NULL...\n");
                }
                spinlock_lock(&base->lock);
                // Removes the timer from the list.
                remove_timer(timer);
                // Free the memory of the timer.
                __timer_list_dealloc(timer);
            }
        }
        // Advance timer check
        ++base->timer_ticks;
    }

    base->running_timer = NULL;

#else

    struct list_head *it, *tmp;
    list_for_each_safe (it, tmp, &base->list) {
        struct timer_list *timer = list_entry(it, struct timer_list, entry);
        if (timer->expires <= timer_get_ticks()) {
            base->running_timer = timer;
            timer->base         = NULL;

            // Executes timer function
            spinlock_unlock(&base->lock);
            pr_debug("Executing dynamic timer function...\n");
            timer->function(timer->data);
            spinlock_lock(&base->lock);

            // Removes timer from list
            pr_debug("Removing dynamic timer...\n");
            list_head_remove(it);
            __timer_list_dealloc(timer);
        }
    }

#endif

    base->running_timer = NULL;
    spinlock_unlock(&base->lock);
}

void init_timer(struct timer_list *timer)
{
    timer->base = NULL;
    list_head_init(&timer->entry);
    spinlock_unlock(&timer->lock);
}

void add_timer(struct timer_list *timer)
{
#ifdef ENABLE_REAL_TIMER_SYSTEM
    // Get the vector.
    list_head *vector = __timer_get_vector(&cpu_base, timer);
    // Insert the timer inside the vector.
    list_head_insert_before(&timer->entry, vector);
    // Debug on the output.
    pr_debug("Add timer     (0x%p) to (0x%p)\n", &timer->entry, vector);
    __print_vector_base(&cpu_base);
#else
    list_head_insert_before(&timer->entry, &base->list);
#endif
}

void remove_timer(struct timer_list *timer)
{
    // First, remove the timer from the current list.
    list_head_remove(&timer->entry);
    // Then, re-initialize the timer.
    init_timer(timer);
    // Debug on the output.
    pr_debug("Remove timer  (0x%p)\n", &timer->entry);
    __print_vector_base(&cpu_base);
}

// ============================================================================
// STANDARD TIMEOUT FUNCTIONS
// ============================================================================

/// @brief Debugging function.
/// @param data The data.
static inline void debug_timeout(unsigned long data)
{
    pr_debug("The timer has been successfully deactivated: %d, ticks: %d, seconds: %d\n", data, timer_ticks, timer_get_seconds());
}

/// @brief Callback for when a sleep timer expires
/// @param data Custom data stored in the timer
static inline void sleep_timeout(unsigned long data)
{
    // TODO: We could modify the sleep_on and make it return the
    // wait_queue_entry_t and then store it in the dynamic timer data member
    // instead of the task pid, this would remove the need to iterate the sleep
    // queue list.
    // Get the sleep data.
    sleep_data_t *sleep_data = (sleep_data_t *)data;
    // Get the wait_queue_entry.
    wait_queue_entry_t *wait_queue_entry = sleep_data->wait_queue_entry;
    // Executed entry's wakeup test function
    if (wait_queue_entry->func(wait_queue_entry, 0, 0) == 1) {
        pr_debug("Process (pid: %d) restored from sleep\n", wait_queue_entry->task->pid);
        // Removes entry from list and memory
        remove_wait_queue(&sleep_queue, wait_queue_entry);
        // Free the memory of the wait queue item.
        wait_queue_entry_dealloc(wait_queue_entry);
        // Free the memory of the sleep_data.
        __sleep_data_dealloc(sleep_data);
    }
}

/// @brief Function executed when the real_timer of a process expires, sends SIGALRM to process.
/// @param pid PID of the process whos associated timer has expired
static inline void alarm_timeout(unsigned long task_ptr)
{
    // Get the task fromt the argument.
    struct task_struct *task = (struct task_struct *)task_ptr;
    // Send ALARM.
    sys_kill(task->pid, SIGALRM);
    // Remove the timer.
    task->real_timer = NULL;
}

// Real timer interval timemout
static inline void real_timer_timeout(unsigned long task_ptr)
{
    // Get the task fromt the argument.
    struct task_struct *task = (struct task_struct *)task_ptr;
    // Send the signal.
    sys_kill(task->pid, SIGALRM);
    // If the real incr is not 0 then restart.
    if (task->it_real_incr != 0) {
        // Create new timer for process, the old one is going to be deleted.
        task->real_timer = __timer_list_alloc();
        // Setup the timer.
        task->real_timer->expires  = timer_get_ticks() + task->it_real_incr;
        task->real_timer->function = &real_timer_timeout;
        task->real_timer->data     = (unsigned long)task;
        // Add the timer.
        add_timer(task->real_timer);
        return;
    }
    // Remove the timer.
    task->real_timer = NULL;
}

// ============================================================================
// TIMING FUNCTIONS
// ============================================================================

int sys_nanosleep(const timespec *req, timespec *rem)
{
    // You probably have to save rem somewhere, because it contains how much
    // time left until the timer expires, when the timer is stopped early by a
    // signal.
    pr_debug("sys_nanosleep([s:%d; ns:%d],...)\n", req->tv_sec, req->tv_nsec);

    // Create a dinamic timer to wake up the process after some time
    struct timer_list *sleep_timer = __timer_list_alloc();

    // Saves pid and rem timespec
    sleep_data_t *data = __sleep_data_alloc();
    data->remaining    = rem;

    // Setup the timer.
    sleep_timer->expires  = timer_get_ticks() + TICKS_PER_SECOND * req->tv_sec;
    sleep_timer->function = &sleep_timeout;
    sleep_timer->data     = (unsigned long)data;
    // Add the timer.
    add_timer(sleep_timer);

    // Removes current process from runqueue and stores it in the waiting queue,
    // this must be done at the end, because it changes the current active page
    // and invalidates the req and rem pointers (?)
    data->wait_queue_entry = sleep_on(&sleep_queue);

    return -1;
}

unsigned sys_alarm(int seconds)
{
    struct task_struct *task = scheduler_get_current_process();

    // If there is already a timer running
    unsigned result = 0;
    if (task->real_timer) {
        remove_timer(task->real_timer);
        result = (task->real_timer->expires - timer_get_ticks()) / TICKS_PER_SECOND;
        // Returns only the amount of seconds remaining
        if (seconds == 0) {
            __timer_list_dealloc(task->real_timer);
            task->real_timer = NULL;
            return result;
        }
    } else {
        if (seconds == 0) {
            return 0;
        }
        // Allocate new timer
        task->real_timer = __timer_list_alloc();
    }

    // Initialize the timer.
    init_timer(task->real_timer);
    // Setup the timer.
    task->real_timer->expires  = timer_get_ticks() + TICKS_PER_SECOND * seconds;
    task->real_timer->function = &alarm_timeout;
    task->real_timer->data     = (unsigned long)task;
    // Add the timer.
    add_timer(task->real_timer);

    return result;
}

static void __calc_itimerval(unsigned long interval, unsigned long value, struct itimerval *result)
{
    result->it_interval.tv_sec  = interval / TICKS_PER_SECOND;
    result->it_interval.tv_usec = (interval * 1000) / TICKS_PER_SECOND;
    result->it_value.tv_sec     = value / TICKS_PER_SECOND;
    result->it_value.tv_usec    = (value * 1000) / TICKS_PER_SECOND;
}

static void __update_task_itimerval(int which, const struct itimerval *timer)
{
    time_t interval = (timer->it_interval.tv_sec * TICKS_PER_SECOND) +
                      (timer->it_interval.tv_usec * TICKS_PER_SECOND) / 1000;
    time_t value = (timer->it_value.tv_sec * TICKS_PER_SECOND) +
                   (timer->it_value.tv_usec * TICKS_PER_SECOND) / 1000;

    struct task_struct *task = scheduler_get_current_process();
    switch (which) {
    case ITIMER_REAL:
        task->it_real_incr  = interval;
        task->it_real_value = value;
        break;
    case ITIMER_VIRTUAL:
        task->it_virt_incr  = interval;
        task->it_virt_value = value;
        break;
    case ITIMER_PROF:
        task->it_prof_incr  = interval;
        task->it_prof_value = value;
        break;
    }
}

int sys_getitimer(int which, struct itimerval *curr_value)
{
    // Invalid time domain
    if (which < 0 || which > 3) {
        return EINVAL;
    }
    struct task_struct *task = scheduler_get_current_process();
    switch (which) {
    case ITIMER_REAL: {
        // Extract remaining time in dynamic timer.
        task->it_real_value = task->real_timer->expires - timer_get_ticks();
        // Compute the interval.
        __calc_itimerval(task->it_real_incr, task->it_real_value, curr_value);
    } break;
    case ITIMER_VIRTUAL:
        // Compute the interval.
        __calc_itimerval(task->it_virt_incr, task->it_virt_value, curr_value);
        break;
    case ITIMER_PROF:
        // Compute the interval.
        __calc_itimerval(task->it_prof_incr, task->it_prof_value, curr_value);
        break;
    }
    return 0;
}

int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value)
{
    // Invalid time domain
    if (which < 0 || which > 3) {
        return EINVAL;
    }
    // Returns old timer interval
    if (old_value != NULL) {
        sys_getitimer(which, old_value);
    }
    // Get ticks of interval.
    time_t new_interval = (new_value->it_interval.tv_sec * TICKS_PER_SECOND) +
                          (new_value->it_interval.tv_usec * TICKS_PER_SECOND) / 1000;
    // If interval is 0 removes timer
    struct task_struct *task = scheduler_get_current_process();
    if (new_interval == 0) {
        // Removes real_timer.
        if ((which == ITIMER_REAL) && (task->real_timer != NULL)) {
            __timer_list_dealloc(task->real_timer);
            task->real_timer = NULL;
        }
        __update_task_itimerval(which, new_value);
        return -1;
    }
    switch (which) {
    // Uses Dynamic Timers
    case ITIMER_REAL: {
        // Remove real_timer if already in use.
        if (task->real_timer) {
            remove_timer(task->real_timer);
        } else {
            // Alloc new timer
            task->real_timer = __timer_list_alloc();
        }
        // Initialize the timer.
        init_timer(task->real_timer);
        // Setup the timer.
        task->real_timer->expires  = timer_get_ticks() + new_interval;
        task->real_timer->function = &real_timer_timeout;
        task->real_timer->data     = (unsigned long)task;
        // Add the timer.
        add_timer(task->real_timer);
    } break;

    case ITIMER_VIRTUAL:
    case ITIMER_PROF:
        break;
    }

    __update_task_itimerval(which, new_value);
    return -1;
}

void update_process_profiling_timer(task_struct *proc)
{
    // If the timer is active
    if (proc->it_prof_incr != 0) {
        proc->it_prof_value += proc->se.exec_runtime;
        if (proc->it_prof_value >= proc->it_prof_incr) {
            sys_kill(proc->pid, SIGPROF);
            proc->it_prof_value = 0;
        }
    }
}
