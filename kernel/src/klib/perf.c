/// @file perf.c
/// @brief The counter facility described in klib/perf.h.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "klib/perf.h"

#ifdef ENABLE_PERF

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PERF  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "hardware/timer.h"
#include "stddef.h"

/// Every counter that has been used at least once, newest first. Built by
/// perf_add() rather than by a registration call, so a counter costs nothing
/// until the code that owns it actually runs.
static perf_counter_t *perf_list = NULL;

/// The cycle counter when the window opened, so a report can say how long it
/// covers without anyone having to measure that separately.
static uint32_t perf_window_cycles = 0;
/// The same, in timer ticks. Two clocks over one window is what turns a cycle
/// count into a duration, and the report prints both rather than doing the
/// division on assumptions about the machine.
static unsigned long perf_window_ticks = 0;

uint32_t perf_now(void)
{
    uint32_t low;
    uint32_t high;
    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
    // A 64-bit value shifted down into 32 bits, done in halves because this
    // kernel's uint64_t is not 64 bits (issue #270).
    return (high << (32U - PERF_CYCLE_SHIFT)) | (low >> PERF_CYCLE_SHIFT);
}

void perf_add(perf_counter_t *counter, uint32_t amount)
{
    if (counter == NULL) {
        return;
    }
    if (!counter->listed) {
        counter->listed = true;
        counter->next   = perf_list;
        perf_list       = counter;
    }
    uint32_t before = counter->total;
    counter->total  = before + amount;
    // The wrap is recorded rather than hidden: a 32-bit total of 256-cycle units
    // is minutes of CPU time, which a long run can genuinely exceed.
    if (counter->total < before) {
        ++counter->wraps;
    }
    if ((counter->samples == 0U) || (amount < counter->smallest)) {
        counter->smallest = amount;
    }
    if (amount > counter->largest) {
        counter->largest = amount;
    }
    ++counter->samples;
}

void perf_reset(void)
{
    for (perf_counter_t *counter = perf_list; counter != NULL; counter = counter->next) {
        counter->total    = 0;
        counter->samples  = 0;
        counter->smallest = 0;
        counter->largest  = 0;
        counter->wraps    = 0;
    }
    perf_window_ticks  = timer_get_ticks();
    perf_window_cycles = perf_now();
}

void perf_report(const char *tag)
{
    if (tag == NULL) {
        tag = "report";
    }
    // The window first, because everything else is read against it. One line per
    // metric, every field named: nothing here depends on column positions.
    pr_notice(
        "PERF %s perf.elapsed unit=%s total=%u samples=1 min=0 max=0 wraps=0\n", tag, PERF_UNIT_CYCLES,
        perf_now() - perf_window_cycles);
    pr_notice(
        "PERF %s perf.ticks unit=ticks total=%u samples=1 min=0 max=0 wraps=0\n", tag,
        (unsigned)(timer_get_ticks() - perf_window_ticks));
    for (perf_counter_t *counter = perf_list; counter != NULL; counter = counter->next) {
        pr_notice(
            "PERF %s %s unit=%s total=%u samples=%u min=%u max=%u wraps=%u\n", tag, counter->name, counter->unit,
            counter->total, counter->samples, counter->smallest, counter->largest, counter->wraps);
    }
}

#endif
