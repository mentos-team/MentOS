/// @file timer.c
/// @brief Timer implementation.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[TIMER ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "hardware/timer.h"

#include "klib/irqflags.h"
#include "process/scheduler.h"
#include "hardware/pic8259.h"
#include "io/port_io.h"
#include "io/debug.h"
#include "io/video.h"
#include "stdint.h"
#include "mem/kheap.h"
#include "process/wait.h"
#include "drivers/rtc.h"
#include "descriptor_tables/isr.h"
#include "devices/fpu.h"
#include "system/signal.h"
#include "assert.h"
#include "sys/errno.h"

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

void timer_install()
{
    dynamic_timers_install();

    // Set the timer phase.
    timer_phase(TICKS_PER_SECOND);
    // Installs 'timer_handler' to IRQ0.
    irq_install_handler(IRQ_TIMER, timer_handler, "timer");
    // Enable the IRQ of the timer.
    pic8259_irq_enable(IRQ_TIMER);
}

uint64_t timer_get_seconds()
{
    return timer_ticks / TICKS_PER_SECOND;
}

unsigned long timer_get_ticks()
{
    return timer_ticks;
}

//======================================================================================
// Dynamics timers

/// Contains timer for each CPU (for now only one)
static tvec_base_t cpu_base = { 0 };

/// Contains all process waiting for a sleep
static wait_queue_head_t sleep_queue;

/// @brief Initialize dynamic timer system
void dynamic_timers_install()
{
#ifndef ENABLE_REAL_TIMER_SYSTEM
    list_head_init(&cpu_base.list);
#endif

    // Initialize tvec_base structure
    tvec_base_t *base = &cpu_base;
    base->timer_ticks = 0;

    for (int i = 0; i < TVR_SIZE; ++i)
        list_head_init(base->tv1.vec + i);

    for (int i = 0; i < TVN_SIZE; ++i) {
        list_head_init(base->tv2.vec + i);
        list_head_init(base->tv3.vec + i);
        list_head_init(base->tv4.vec + i);
        list_head_init(base->tv5.vec + i);
    }

    // Initialize sleeping process list
    list_head_init(&sleep_queue.task_list);
    spinlock_init(&sleep_queue.lock);
}

/// Prints used slots of timer vector
static void __print_tvec_slots(tvec_base_t *base, int tv_index)
{
    if (tv_index < 0 || tv_index > 5)
        return;

    // Write buffer
    char result[TVN_SIZE + 1];
    result[TVN_SIZE] = '\0';

    struct timer_vec *tv = NULL;
    switch (tv_index) {
    // Root
    case 1: {
        pr_debug("base->tv1.vec:");
        for (int i = 0; i < TVR_SIZE; ++i) {
            // New line in order to not clutter the screen
            int index = i % TVN_SIZE;
            if (i != 0 && index == 0)
                pr_debug("\n\t%s", result);

            if (!list_head_empty(base->tv1.vec + i))
                result[index] = '1';
            else
                result[index] = '0';
        }

        // The last line
        pr_debug("\n\t%s\n", result);
        return;

    } break;

    // Normal
    case 2:
        tv = &base->tv2;
        break;
    case 3:
        tv = &base->tv3;
        break;
    case 4:
        tv = &base->tv4;
        break;
    case 5:
        tv = &base->tv5;
        break;
    }

    for (int i = 0; i < TVN_SIZE; ++i) {
        if (list_head_empty(tv->vec + i))
            result[i] = '0';
        else
            result[i] = '1';
    }

    pr_debug("base->tv%d.vec:\n\t%s\n", tv_index, result);
    (void) result;
}

/// Dump all timer vector in base
static inline void __dump_all_tvec_slots(tvec_base_t *base)
{
    __print_tvec_slots(base, 1);
    __print_tvec_slots(base, 2);
    __print_tvec_slots(base, 3);
    __print_tvec_slots(base, 4);
    __print_tvec_slots(base, 5);
}

/// Select correct timer vector and position inside of it for the input timer
/// index contains the position inside of the tv_index timer vector
static void __find_tvec(tvec_base_t *base, struct timer_list *timer, int *index, int *tv_index)
{
    assert(index && "index is NULL");
    assert(tv_index && "tv_index is NULL");

    unsigned long expires = timer->expires;
    unsigned long ticks   = expires - base->timer_ticks;

    unsigned long tv1_ticks = TIMER_TICKS(0);
    unsigned long tv2_ticks = TIMER_TICKS(1);
    unsigned long tv3_ticks = TIMER_TICKS(2);
    unsigned long tv4_ticks = TIMER_TICKS(3);

    // Can happen if you add a timer with expires == ticks, or in the past
    if ((signed long)ticks < 0) {
        *index    = base->timer_ticks & TVR_MASK;
        *tv_index = 1;
    }
    // tv1
    else if (ticks < tv1_ticks) {
        *index    = expires & TVR_MASK;
        *tv_index = 1;
    }
    // tv2
    else if (ticks < tv2_ticks) {
        *index    = (expires >> TIMER_TICKS_BITS(0)) & TVN_MASK;
        *tv_index = 2;
    }
    // tv3
    else if (ticks < tv3_ticks) {
        *index    = (expires >> TIMER_TICKS_BITS(1)) & TVN_MASK;
        *tv_index = 3;
    }
    // tv4
    else if (ticks < tv4_ticks) {
        *index    = (expires >> TIMER_TICKS_BITS(2)) & TVN_MASK;
        *tv_index = 4;
    }
    // tv5
    else {
        *index    = (expires >> TIMER_TICKS_BITS(3)) & TVN_MASK;
        *tv_index = 5;
    }
}

/// Add timers into different lists based on their expire time
static void __add_timer_tvec_base(tvec_base_t *base, struct timer_list *timer)
{
    int index = 0, tv_index = 0;
    __find_tvec(base, timer, &index, &tv_index);

    struct list_head *vec;
    switch (tv_index) {
    case 1:
        vec = base->tv1.vec + index;
        break;
    case 2:
        vec = base->tv2.vec + index;
        break;
    case 3:
        vec = base->tv3.vec + index;
        break;
    case 4:
        vec = base->tv4.vec + index;
        break;
    case 5:
        vec = base->tv5.vec + index;
        break;
    }

    pr_debug("Adding timer at time_index: %d in tv%d\n", index, tv_index);
    list_head_insert_before(&timer->entry, vec);

#ifdef ENABLE_REAL_TIMER_SYSTEM_DUMP
    __dump_all_tvec_slots(base);
#endif
}

/// Remove timer from tvec_base
static void __rem_timer_tvec_base(tvec_base_t *base, struct timer_list *timer)
{
    int index = 0, tv_index = 0;
    __find_tvec(base, timer, &index, &tv_index);

    // TODO: Check why we do not use vec.
    struct list_head *vec;
    switch (tv_index) {
    case 1:
        vec = base->tv1.vec + index;
        break;
    case 2:
        vec = base->tv2.vec + index;
        break;
    case 3:
        vec = base->tv3.vec + index;
        break;
    case 4:
        vec = base->tv4.vec + index;
        break;
    case 5:
        vec = base->tv5.vec + index;
        break;
    }

    pr_debug("Removing timer at time_index: %d in tv%d\n", index, tv_index);
    list_head_remove(&timer->entry);

#ifdef ENABLE_REAL_TIMER_SYSTEM_DUMP
    __dump_all_tvec_slots(base);
#endif
    (void)vec;
}

/// Move all timers from tv up one level
static int cascate(tvec_base_t *base, timer_vec *tv, int time_index, int tv_index)
{
    if (!list_head_empty(tv->vec + time_index)) {
        pr_debug("Relocating timers in tv%d.vec[%d]\n", tv_index, time_index);

        // Reinsert all timers into base in the new correct list
        struct list_head *it, *tmp;
        list_for_each_safe (it, tmp, tv->vec + time_index) {
            struct timer_list *timer = list_entry(it, struct timer_list, entry);
            list_head_remove(it);

            __add_timer_tvec_base(base, timer);
        }
    }
    return time_index;
}

void run_timer_softirq()
{
    tvec_base_t *base = &cpu_base;
    spinlock_lock(&base->lock);

#ifdef ENABLE_REAL_TIMER_SYSTEM

    // While we are not up to date with current ticks
    unsigned long current_ticks = timer_get_ticks();
    while (base->timer_ticks <= current_ticks) {
        // Index of the current timer to execute
        int current_time_index = base->timer_ticks & TVR_MASK;

        // If the index is zero then all lists in base->tv1 have been checked, so they are empty
        if (!current_time_index) {
            // Consider the first invocation of the cascade() function: it receives as arguments
            // the address in base, the address of base->tv2, and the index of the list
            // in base->tv2 including the timers that will decay in the next 256 ticks. This
            // index is determined by looking at the proper bits of the base->timer_ticks value.
            // cascade() moves all dynamic timers in the base->tv2 list into the
            // proper lists of base->tv1; then, it returns a positive value, unless all base->tv2
            // lists are now empty. If so, cascade() is invoked once more to replenish
            // base->tv2 with the timers included in a list of base->tv3, and so on.

            int tv2_index = (base->timer_ticks >> TIMER_TICKS_BITS(0)) & TVN_MASK;
            int tv3_index = (base->timer_ticks >> TIMER_TICKS_BITS(1)) & TVN_MASK;
            int tv4_index = (base->timer_ticks >> TIMER_TICKS_BITS(2)) & TVN_MASK;
            int tv5_index = (base->timer_ticks >> TIMER_TICKS_BITS(3)) & TVN_MASK;

            if (!cascate(base, &base->tv2, tv2_index, 2) &&
                !cascate(base, &base->tv3, tv3_index, 3) &&
                !cascate(base, &base->tv4, tv4_index, 4) &&
                !cascate(base, &base->tv5, tv5_index, 5))
                ;
        }

        // If there are timers to execute in this instant
        if (!list_head_empty(&base->tv1.vec[current_time_index])) {
            pr_debug("Executing dynamic timers at %d ticks from start inside of tv1.vec[%d]\n",
                     base->timer_ticks, current_time_index);

            // Trigger all timers
            struct list_head *it, *tmp;
            list_for_each_safe (it, tmp, &base->tv1.vec[current_time_index]) {
                struct timer_list *timer = list_entry(it, struct timer_list, entry);

                // Executes timer function
                spinlock_unlock(&base->lock);
                pr_debug("Executing dynamic timer function...\n");
                timer->function(timer->data);
                spinlock_lock(&base->lock);

                // Removes timer from list
                list_head_remove(it);
                kfree(timer);
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
            kfree(timer);
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
    tvec_base_t *base = &cpu_base;
    timer->base       = base;

#ifdef ENABLE_REAL_TIMER_SYSTEM
    __add_timer_tvec_base(base, timer);
#else
    list_head_insert_before(&timer->entry, &base->list);
#endif
}

void del_timer(struct timer_list *timer)
{
    tvec_base_t *base = &cpu_base;
    timer->base       = NULL;

#ifdef ENABLE_REAL_TIMER_SYSTEM
    __rem_timer_tvec_base(base, timer);
#else
    list_head_remove(&timer->entry);
#endif
}

//======================================================================================
// Sleep

/// @brief Debugging function.
/// @param data The data.
static inline void debug_timeout(unsigned long data)
{
    pr_debug("The timer has been successfully deactivated: %d, ticks: %d, seconds: %d\n", data, timer_ticks, timer_get_seconds());
}

/// @brief Contains the entry of a wait queue and timespec which keeps trakc of
///        the remaining time.
typedef struct sleep_data_t {
    /// POinter to the entry of a wait queue.
    wait_queue_entry_t *entry;
    /// Keeps track of the remaining time.
    timespec *rem;
} sleep_data_t;

/// @brief Callback for when a sleep timer expires
/// @param data Custom data stored in the timer
void sleep_timeout(unsigned long data)
{
    // NOTE: We could modify the sleep_on and make it return the wait_queue_entry_t
    //       and then store it in the dynamic timer data member instead of the task pid,
    //       this would remove the need to iterate the sleep queue list.

    sleep_data_t *sleep_data = (sleep_data_t *)data;

    wait_queue_entry_t *entry = sleep_data->entry;
    task_struct *task         = entry->task;

    // Executed entry's wakeup test function
    int res = entry->func(entry, 0, 0);
    if (res == 1) {
        // Removes entry from list and memory
        remove_wait_queue(&sleep_queue, entry);
        kfree(entry);

        pr_debug("Process (pid: %d) restored from sleep\n", task->pid);
    }
}

int sys_nanosleep(const timespec *req, timespec *rem)
{
    // Probabilmente devi salvare rem da qualche parte, perche' dentro ci va
    // messo quanto tempo mancava allo scadere del timer nel caso in cui il
    // timer venga interrotto prima da un segnale.
    pr_debug("sys_nanosleep([s:%d; ns:%d],...)\n", req->tv_sec, req->tv_nsec);

    // Saves pid and rem timespec
    sleep_data_t *data = kmalloc(sizeof(sleep_data_t));
    data->rem          = rem;

    // Create a dinamic timer to wake up the process after some time
    struct timer_list *sleep_timer = kmalloc(sizeof(struct timer_list));
    init_timer(sleep_timer);

    sleep_timer->expires  = timer_get_ticks() + TICKS_PER_SECOND * req->tv_sec;
    sleep_timer->function = &sleep_timeout;
    sleep_timer->data     = (unsigned long)data;

    // Removes current process from runqueue and stores it in the waiting queue,
    // this must be done at the end, because it changes the current active page
    // and invalidates the req and rem pointers (?)
    wait_queue_entry_t *entry = sleep_on(&sleep_queue);
    data->entry               = entry;

    add_timer(sleep_timer);
    return -1;
}

/// @brief Function executed when the real_timer of a process expires, sends SIGALRM to process.
/// @param pid PID of the process whos associated timer has expired
void alarm_timeout(unsigned long pid)
{
    sys_kill(pid, SIGALRM);

    struct task_struct *cur = scheduler_get_current_process();
    cur->real_timer         = NULL;
}

int sys_alarm(int seconds)
{
    pr_debug("sys_alarm(seconds:%d)\n", seconds);

    struct task_struct *current = scheduler_get_current_process();
    struct timer_list *timer;

    // If there is already a timer running
    int result = 0;
    if (current->real_timer != NULL) {
        del_timer(current->real_timer);
        result = (current->real_timer->expires - timer_get_ticks()) / TICKS_PER_SECOND;
        timer  = current->real_timer;

        // Returns only the amount of seconds remaining
        if (seconds == 0) {
            kfree(current->real_timer);
            current->real_timer = NULL;
            return result;
        }
    } else {
        if (seconds == 0)
            return 0;

        // Allocate new timer
        timer = (struct timer_list *)kmalloc(sizeof(struct timer_list));
    }

    current->real_timer = timer;
    init_timer(timer);

    timer->expires  = timer_get_ticks() + TICKS_PER_SECOND * seconds;
    timer->function = &alarm_timeout;
    timer->data     = current->pid;

    add_timer(timer);

    return result;
}

static void calc_itimerval(unsigned long incr, unsigned long value, struct itimerval *result)
{
    result->it_interval.tv_sec  = incr / TICKS_PER_SECOND;
    result->it_interval.tv_usec = incr / TICKS_PER_SECOND * 1000;

    result->it_value.tv_sec  = value / TICKS_PER_SECOND;
    result->it_value.tv_usec = value / TICKS_PER_SECOND * 1000;
}

static void update_task_itimerval(int which, const struct itimerval *val)
{
    unsigned long interval_ticks = val->it_interval.tv_sec * TICKS_PER_SECOND;
    interval_ticks += val->it_interval.tv_usec * TICKS_PER_SECOND / 1000;

    unsigned long value_ticks = val->it_value.tv_sec * TICKS_PER_SECOND;
    value_ticks += val->it_value.tv_usec * TICKS_PER_SECOND / 1000;

    struct task_struct *curr = scheduler_get_current_process();
    switch (which) {
    case ITIMER_REAL:
        curr->it_real_incr  = interval_ticks;
        curr->it_real_value = value_ticks;
        break;

    case ITIMER_VIRTUAL:
        curr->it_virt_incr  = interval_ticks;
        curr->it_virt_value = value_ticks;
        break;

    case ITIMER_PROF:
        curr->it_prof_incr  = interval_ticks;
        curr->it_prof_value = value_ticks;
        break;
    }
}

int sys_getitimer(int which, struct itimerval *curr_value)
{
    // Invalid time domain
    if (which < 0 || which > 3)
        return EINVAL;

    struct task_struct *curr = scheduler_get_current_process();
    switch (which) {
    case ITIMER_REAL: {
        // Extract remaining time in dynamic timer
        unsigned long value = curr->real_timer->expires - timer_get_ticks();
        curr->it_real_value = value;
        calc_itimerval(curr->it_real_incr, curr->it_real_value, curr_value);
    } break;
    case ITIMER_VIRTUAL:
        calc_itimerval(curr->it_virt_incr, curr->it_virt_value, curr_value);
        break;
    case ITIMER_PROF:
        calc_itimerval(curr->it_prof_incr, curr->it_prof_value, curr_value);
        break;
    }
    return 0;
}

// Real timer interval timemout
static void it_real_fn(unsigned long pid)
{
    struct task_struct *cur = scheduler_get_running_process(pid);
    sys_kill(pid, SIGALRM);

    // If the real incr is not 0 then restart
    if (cur->it_real_incr != 0) {
        // Create new timer for process
        struct timer_list *real_timer = (struct timer_list *)kmalloc(sizeof(struct timer_list));
        cur->real_timer               = real_timer;
        init_timer(real_timer);

        real_timer->expires  = timer_get_ticks() + cur->it_real_incr;
        real_timer->function = &it_real_fn;
        real_timer->data     = cur->pid;

        add_timer(real_timer);
        return;
    }

    // No more timer
    cur->real_timer = NULL;
}

int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value)
{
    // Invalid time domain
    if (which < 0 || which > 3)
        return EINVAL;

    // Returns old timer interval
    if (old_value != NULL)
        sys_getitimer(which, old_value);

    // Get ticks of interval
    unsigned long interval_ticks = new_value->it_interval.tv_sec * TICKS_PER_SECOND;
    interval_ticks += new_value->it_interval.tv_usec * TICKS_PER_SECOND / 1000;

    // If interval is 0 removes timer
    struct task_struct *cur = scheduler_get_current_process();
    if (interval_ticks == 0) {
        // Removes real_timer
        if (which == ITIMER_REAL && cur->real_timer != NULL)
            cur->real_timer = NULL;

        update_task_itimerval(which, new_value);
        return -1;
    }

    switch (which) {
    // Uses Dynamic Timers
    case ITIMER_REAL: {
        // Remove real_timer if already in use
        struct timer_list *timer = cur->real_timer;
        if (timer != NULL) {
            del_timer(timer); // Recycle memory
        } else {
            // Alloc new timer
            timer = (struct timer_list *)kmalloc(sizeof(struct timer_list));
        }

        init_timer(timer);

        timer->expires  = timer_get_ticks() + interval_ticks;
        timer->function = &it_real_fn;
        timer->data     = cur->pid;

        add_timer(timer);

    } break;

    case ITIMER_VIRTUAL:
    case ITIMER_PROF:
        break;
    }

    update_task_itimerval(which, new_value);
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
