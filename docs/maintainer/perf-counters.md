# Performance counters

`klib/perf.h` is the whole facility: a named accumulator, and two ways to feed
it. It exists because every performance investigation in this kernel has so far
started by writing the same throwaway probe -- a static counter, an `rdtsc`
around something, a `pr_notice` at the end -- and throwing it away again.

Off by default. `cmake -DENABLE_PERF=ON` turns it on; with it off every macro
expands to nothing that costs anything and no counter exists.

## Using it

A subsystem declares the metrics it cares about beside the code that produces
them. There is no central table to extend and no field to add to a shared
structure, which is the point: the core knows nothing about video, virtio, ATA or
anything else.

```c
#include "klib/perf.h"

PERF_COUNTER(ata_reads, "ata.read.calls", "calls");
PERF_COUNTER(ata_bytes, "ata.read.bytes", "bytes");
PERF_COUNTER(ata_cycles, "ata.read.cycles", PERF_UNIT_CYCLES);

int ata_read(...)
{
    perf_scope_t scope = PERF_SCOPE_BEGIN();
    PERF_INC(ata_reads);
    ...
    PERF_ADD(ata_bytes, count);
    PERF_SCOPE_END(ata_cycles, scope);
}
```

Names are dotted `subsystem.thing.unit`, so a report sorts and greps sensibly.
Counters register themselves the first time they are used, so one that never runs
never appears.

## Measuring

```sh
echo reset > /proc/perf     # start a window
<run the workload>
cat /proc/perf              # counters go to the kernel log
```

The counters go to the *kernel log* rather than into the `read()` buffer, because
a procfs read here is bounded by one `BUFSIZ` and there can be any number of
counters. What the read returns is only confirmation that a report was taken.

One line per metric, every field named, nothing positional:

```
PERF <tag> <name> unit=<unit> total=<n> samples=<n> min=<n> max=<n> wraps=<n>
```

`samples` is how many additions made up `total`, so `total / samples` is the mean
cost of whatever was counted, and `min`/`max` bound it. Every report opens with
`perf.elapsed` and `perf.ticks` for the window itself.

## Units, and the two clocks

A counter carries its unit as a string and the report prints it. Nothing infers
that a number is cycles, bytes or events, and nothing calls a cycle count a
duration.

Cycles are counted in **units of 256** (`PERF_UNIT_CYCLES`, spelled
`cycles256`), because this kernel's `uint64_t` is 32 bits wide (issue #270): a
raw cycle count would wrap in seconds, this wraps in minutes, and a wrap is
counted in `wraps` rather than silently absorbed. Multiply by 256 for cycles.

**Do not divide `perf.elapsed` by `perf.ticks` to get a clock rate under load.**
The ticks come from the PIT, and a processor stalled in device emulation misses
interrupts. Measured on a console workload: the timer had advanced 0.45 s while
the cycle counter had advanced 30 s, and the cycle counter was right -- the guest
simply does not perceive time it spent inside the host. Calibrate on an idle
window instead, a userspace `sleep(1)`, where the two agree; that is also the
number to use when converting a measurement into the seconds a human would
experience.

## Guarantees, stated rather than assumed

- **Not atomic, not locked.** A counter is a 32-bit read-modify-write. On this
  single-processor kernel an addition from an interrupt that lands between the
  read and the write of one from process context is lost. The counter cannot be
  corrupted -- an aligned 32-bit store is atomic on i386 -- so the failure mode is
  an undercount of one addition. Counters are therefore approximate, deliberately:
  the alternative distorts the timing of the code being measured.
- **Safe from interrupt context** on those terms, and used that way.
- **Nested and recursive scopes work**, because a scope keeps its start stamp in
  the caller's own variable and the core holds no scope state.
- **If MentOS becomes SMP**, the same reasoning gives the same undercount between
  processors. Per-CPU counters are the answer then, not a lock.
- **Reporting is not for hot paths.** It prints. Collect during the workload,
  report after it.

## What it costs

Measured at **344 cycles** for one begin/end/add triple, in QEMU's interpreter,
where almost all of it is the two `rdtsc` traps -- about 25 cycles on real
hardware. Against 40000 characters through the console that is **0.015%** of the
window for one scope per character, which is why the current counters can sit in
the per-character path without moving the numbers they report.

An individual sample is not trustworthy at that granularity: an interrupt landing
inside a scope shows up as a wild `max`. Totals over thousands of samples are.

## What is instrumented today

Enough to catch a regression in the paths that have already produced one, and no
more.

| metric | says |
|---|---|
| `video.putc.calls` / `.cycles` | what one character through the console costs |
| `video.put_cells.calls` / `.cells` / `.cycles` | the amplification: cells per call is how much of a row one character dirties |
| `video.set_cursor.calls` / `.cycles` | cursor re-placement, which used to publish per character |
| `video.scroll.calls` / `.cycles` | scrolling, currently the most expensive thing the console does |
| `video.batch.runs` | how many runs of output there were; against `put_cells.calls` it shows batching working |
| `video.resize.calls` / `.cycles` | a geometry change, end to end |
| `virtio_gpu.draw_cell.cells` | glyphs rasterized into guest RAM |
| `virtio_gpu.publish.calls` / `.cycles` | how often the backend went to the device, and for how long |
| `virtio_gpu.transfer.calls` / `.bytes256` | TRANSFER_TO_HOST_2D commands, and how much the host had to copy |
| `virtio_gpu.resource_flush.calls` | RESOURCE_FLUSH commands |
| `virtqueue.submit.calls` / `.poll_rounds` / `.cycles` | device round trips, and how long the processor spent waiting |

`bytes256` is bytes in units of 256, for the same 32-bit reason as cycles.
