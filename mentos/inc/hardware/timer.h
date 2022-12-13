/// @file timer.h
/// @brief Programmable Interval Timer (PIT) definitions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "kernel.h"
#include "stdint.h"
#include "klib/list_head.h"
#include "klib/spinlock.h"
#include "process/process.h"
#include "time.h"

/// This enables the dynamic timer system use an hierarchical timing wheel,
/// an optimized data structure that allows O(1) insertion, O(1) deletion and
/// ammortized O(1) tick update.
#define ENABLE_REAL_TIMER_SYSTEM

/// This enables the system dump tvec_base timer vectors content on
/// the console.
#define ENABLE_REAL_TIMER_SYSTEM_DUMP

/// Counts down in real (i.e., wall clock) time.
#define ITIMER_REAL 0
/// Counts down against the user-mode CPU time consumed by the process.
#define ITIMER_VIRTUAL 1
/// This timer counts down against the total (i.e., both user and system) CPU
/// time consumed by the process.
#define ITIMER_PROF 2

/// Number of ticks per seconds.
#define TICKS_PER_SECOND 1193

/// @brief   Handles the timer.
/// @param f The interrupt stack frame.
/// @details
/// In this case, it's very simple: We increment the 'timer_ticks' variable
/// every time the timer fires. By default, the timer fires 18.222 times
/// per second. Why 18.222Hz? Some engineer at IBM must've been smoking
/// something funky.
void timer_handler(pt_regs *f);

/// @brief Sets up the system clock by installing the timer handler into IRQ0.
void timer_install();

/// @brief Returns the number of seconds since the system started its execution.
/// @return Value in seconds.
uint64_t timer_get_seconds();

/// @brief Returns the number of ticks since the system started its execution.
/// @return Value in ticks.
unsigned long timer_get_ticks();

/// @brief Allows to set the timer phase to the given frequency.
/// @param hz The frequency to set.
void timer_phase(const uint32_t hz);

// ===============================================================================
// Per-CPU timer vectors

#ifdef ENABLE_REAL_TIMER_SYSTEM

/// Number of bits for the normal timer vector.
#define TVN_BITS 6
/// Number of bits for the root timer vector.
#define TVR_BITS 8
/// Number of headers in a normal timer vector.
#define TVN_SIZE (1 << TVN_BITS)
/// Number of headers in a root timer vector.
#define TVR_SIZE (1 << TVR_BITS)
/// A mask with all 1s for the normal timer vector (0b00111111)
#define TVN_MASK (TVN_SIZE - 1)
/// A mask with all 1s for the root timer vector   (0b11111111)
#define TVR_MASK (TVR_SIZE - 1)
/// A shift used to calculate a timer position inside the tvec_base structure
#define TIMER_TICKS_BITS(tv) (TVR_BITS + TVN_BITS * (tv))
/// Expiration ticks of timer based on position inside tvec_base structure
#define TIMER_TICKS(tv) (1 << TIMER_TICKS_BITS(tv))

/// @brief Root timer vector.
typedef struct timer_vec_root {
    /// Array of lists of timers
    list_head vec[TVR_SIZE];
} timer_vec_root;

/// @brief Normal timer vector.
typedef struct timer_vec {
    /// Array of lists of timers
    list_head vec[TVN_SIZE];
} timer_vec;

#endif

/// @brief Contains all the timers of a single CPU
typedef struct tvec_base_s {
    /// Lock for the timer data structure
    spinlock_t lock;
    /// Points to the dynamic timer that is currently handled by the CPU.
    struct timer_list *running_timer;
#ifdef ENABLE_REAL_TIMER_SYSTEM
    /// The earliest expiration time of the dynamic timers yet to be checked
    unsigned long timer_ticks;

    /// Lists of timers that will expires in the next 255 ticks
    struct timer_vec_root tv1;
    /// Lists of timers that will expires in the next 2^14 - 1 ticks
    struct timer_vec tv2;
    /// Lists of timers that will expires in the next 2^20 - 1 ticks
    struct timer_vec tv3;
    /// Lists of timers that will expires in the next 2^26 - 1 ticks
    struct timer_vec tv4;
    /// Lists of timers with extremely large expires fields (2^32 - 1 ticks)
    struct timer_vec tv5;

#else
    /// List of all the timers
    struct list_head list;
#endif

} tvec_base_t;

/// @brief Represents the request to execute a function in the future, also
///        known as timer.
struct timer_list {
    /// Protects the access to the timer.
    spinlock_t lock;
    /// Lists of timers are mantained using the list_head.
    struct list_head entry;
    /// Ticks value when the timer has to expire
    unsigned long expires;
    /// Functions to be executed when the timer expires
    void (*function)(unsigned long);
    /// Custom data to be passed to the timer function
    unsigned long data;
    /// Pointer to the structure containing all the other related timers.
    tvec_base_t *base;
};

/// @brief Initialize dynamic timer system
void dynamic_timers_install();

/// @brief Initializes a new timer struct.
/// @param timer The timer to initialize.
void init_timer(struct timer_list *timer);

/// @brief Updates the timer data structures
void run_timer_softirq();

/// @brief Add a new timer to the current CPU.
/// @param timer The timer to add.
void add_timer(struct timer_list *timer);

/// @brief Removes a timer from the current CPU.
/// @param timer The timer to remove.
void del_timer(struct timer_list *timer);

/// @brief Updates and executes dynamics timers
void run_timer_softirq();

/// @brief Suspends the execution of the calling thread.
/// @param req The amount of time we want to sleep.
/// @param rem The remaining time we did not sleep.
/// @return If the call is interrupted by a signal handler, nanosleep()
///         returns -1, sets errno to EINTR, and writes the remaining time
///         into the structure pointed to by rem unless rem is NULL.
/// @details
/// The execution is suspended until either at least the time specified
/// in *req has elapsed, or the delivery of a signal that triggers the
/// invocation of a handler in the calling thread or that terminates
/// the process.
int sys_nanosleep(const timespec *req, timespec *rem);

/// @brief Send signal to calling thread after desired seconds.
/// @param seconds The number of seconds in the interval
/// @return the number of seconds remaining until any previously scheduled
///         alarm was due to be delivered, or zero if there was no previously
///         scheduled alarm.
int sys_alarm(int seconds);

/// @brief Rappresents a time value.
struct timeval {
    time_t tv_sec;  ///< Seconds.
    time_t tv_usec; ///< Microseconds.
};

/// @brief Rappresents a time interval.
struct itimerval {
    struct timeval it_interval; ///< Next value.
    struct timeval it_value;    ///< Current value.
};

/// @brief Fills the structure pointed to by curr_value with the current setting for the timer specified by which.
/// @param which      The time domain (i.e., ITIMER_REAL, ITIMER_VIRTUAL, ITIMER_PROF)
/// @param curr_value There the values must be stored.
/// @return Zero on success, or a negative value indicating the error.
int sys_getitimer(int which, struct itimerval *curr_value);

/// @brief Arms or disarms the timer specified by which, by setting the timer
///        to the value specified by new_value.
/// @details
/// The system provides each process with three interval timers, each
///  decrementing in a distinct time domain. When any timer expires,
///  a signal is sent to the process, and the timer (potentially) restarts.
/// @param which     The time domain (i.e., ITIMER_REAL, ITIMER_VIRTUAL, ITIMER_PROF)
/// @param new_value The new value to set.
/// @param old_value Where the old value must be stored.
/// @return Zero on success, or a negative value indicating the error.
int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value);

/// @brief Update the profiling timer and generate SIGPROF if it has expired.
/// @param proc The process for which we must update the profiling.
void update_process_profiling_timer(task_struct *proc);
