/// @file perf.h
/// @brief A small counter facility for answering "where did the cycles go?".
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// The whole facility is a named accumulator and two ways to feed it: add a
/// number, or time a scope. A subsystem declares the metrics it cares about
/// beside the code that produces them, and the core knows nothing about any of
/// them -- there is no central list to extend, and no field to add to a shared
/// structure.
///
/// @code
/// PERF_COUNTER(ata_reads, "ata.read.calls", "calls");
/// PERF_COUNTER(ata_bytes, "ata.read.bytes", "bytes");
/// PERF_COUNTER(ata_time, "ata.read.cycles", PERF_UNIT_CYCLES);
///
/// int ata_read(...)
/// {
///     perf_scope_t scope = PERF_SCOPE_BEGIN();
///     PERF_INC(ata_reads);
///     ...
///     PERF_ADD(ata_bytes, count);
///     PERF_SCOPE_END(ata_time, scope);
/// }
/// @endcode
///
/// **Units are never guessed.** A counter carries the name of its unit as a
/// string and the report prints it, so nothing has to assume that a number is
/// cycles, bytes or events -- and nothing pretends a cycle count is elapsed time.
/// Cycles are counted in units of 256 (see PERF_UNIT_CYCLES) because this
/// kernel's `uint64_t` is 32 bits wide (issue #270); a raw cycle count would wrap
/// in seconds, where this wraps in minutes, and a wrap is *counted* rather than
/// silently absorbed.
///
/// **What it guarantees.** Nothing is locked. Counters are plain 32-bit
/// read-modify-write, which on this single-processor kernel means an addition
/// from an interrupt that lands between the read and the write of one from
/// process context is lost. The counter cannot be corrupted -- an aligned 32-bit
/// store is atomic on i386 -- so the failure mode is an undercount of one
/// addition, not a nonsense value. That is a deliberate trade: counters are
/// approximate, and profiling never changes the timing of what it measures more
/// than it has to. Should MentOS become SMP, the same reasoning gives a
/// per-counter race between processors and the same undercount; if exact totals
/// are ever needed, that is the point to add per-CPU counters, not a lock.
///
/// Scopes keep their start stamp in the caller's own variable, so nesting and
/// recursion work with no state in the core. Reporting is not for hot paths: it
/// prints, so it belongs after the workload, not inside it.
///
/// **What it costs.** Measured in QEMU's interpreter at 344 cycles for one
/// begin/end/add triple, which is almost entirely the two `rdtsc` instructions --
/// a trap there, about 25 cycles on real hardware. Against a workload of 40000
/// characters through the console that is 0.015% of the window for one scope per
/// character, which is why the counters below can sit in the per-character path
/// without moving the numbers they report. An individual sample is not
/// trustworthy at that scale -- an interrupt landing inside a scope shows up as a
/// wild `max` -- but totals over thousands of samples are.
///
/// **When ENABLE_PERF is off**, every macro here expands to nothing that costs
/// anything: no counters are defined, no calls are made, and the arguments are
/// evaluated only where they must be to stay warning-free. The build option
/// defaults to off, so a normal build pays for none of this.

#pragma once

#include "stdbool.h"
#include "stdint.h"

/// @brief The unit string for a cycle counter, spelled once.
///
/// Cycles are accumulated in units of 256, so a figure printed with this unit
/// must be multiplied by 256 to be cycles. Turning that into seconds needs a
/// cycles-per-second for the machine, and every report prints `perf.elapsed`
/// beside `perf.ticks` so the two clocks can be compared -- but **do not divide
/// one by the other under load**. The ticks come from the PIT, and a processor
/// stalled in device emulation misses interrupts: on a console workload measured
/// here the timer had advanced 0.45 s while the cycle counter had advanced 30 s,
/// and the cycle counter was the one telling the truth. Calibrate on an idle
/// window -- a userspace `sleep(1)` -- where nothing is stalled and the two
/// agree.
#define PERF_UNIT_CYCLES "cycles256"

/// @brief How far a cycle count is shifted down before being accumulated.
#define PERF_CYCLE_SHIFT 8U

/// @brief One named accumulator.
///
/// Defined by whoever measures, never by this file. Zero-initialized static
/// storage is a valid unused counter, which is what lets PERF_COUNTER() be a
/// plain definition.
typedef struct perf_counter_t {
    const char *name;            ///< Dotted metric name, e.g. "ext2.lookup.cycles".
    const char *unit;            ///< What one unit of `total` is: "calls", "bytes", PERF_UNIT_CYCLES.
    uint32_t total;              ///< Accumulated value.
    uint32_t samples;            ///< How many additions made it up.
    uint32_t smallest;           ///< Smallest single addition.
    uint32_t largest;            ///< Largest single addition.
    uint32_t wraps;              ///< Times `total` wrapped past 32 bits.
    bool_t listed;               ///< Whether it has joined the report list.
    struct perf_counter_t *next; ///< Next counter to report.
} perf_counter_t;

/// @brief A scope's start stamp, in the same units as a cycle counter.
typedef uint32_t perf_scope_t;

#ifdef ENABLE_PERF

/// @brief Defines a counter.
#define PERF_COUNTER(symbol, metric, units)                                                                            \
    static perf_counter_t symbol = {.name = (metric), .unit = (units)}
/// @brief Counts one event.
#define PERF_INC(symbol)             perf_add(&(symbol), 1U)
/// @brief Adds a quantity: bytes, cells, sectors, whatever the unit says.
#define PERF_ADD(symbol, amount)     perf_add(&(symbol), (uint32_t)(amount))
/// @brief Starts timing a scope. The result belongs in a local variable.
#define PERF_SCOPE_BEGIN()           perf_now()
/// @brief Adds the cycles a scope took to a counter.
#define PERF_SCOPE_END(symbol, scope) perf_add(&(symbol), perf_now() - (scope))

/// @brief The cycle counter, in units of 1 << PERF_CYCLE_SHIFT cycles.
uint32_t perf_now(void);

/// @brief Adds to a counter, registering it for reporting on first use.
void perf_add(perf_counter_t *counter, uint32_t amount);

/// @brief Zeroes every counter and restarts the report window.
void perf_reset(void);

/// @brief Prints every counter that has been used, one line each.
/// @param tag A word identifying the measurement, printed on every line.
void perf_report(const char *tag);

#else

#define PERF_COUNTER(symbol, metric, units) typedef int perf_disabled_##symbol##_t
#define PERF_INC(symbol)                    ((void)0)
#define PERF_ADD(symbol, amount)            ((void)(amount))
#define PERF_SCOPE_BEGIN()                  0U
#define PERF_SCOPE_END(symbol, scope)       ((void)(scope))

static inline uint32_t perf_now(void) { return 0U; }
static inline void perf_reset(void) { }
static inline void perf_report(const char *tag) { (void)tag; }

#endif
