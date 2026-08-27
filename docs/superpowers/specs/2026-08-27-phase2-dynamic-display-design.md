# Phase 2 design: dynamic display sizing

Design investigation only. Nothing here is implemented. Written against
`4da0c6b`, QEMU 8.2.2, on the project's own emulator flags.

Three kinds of statement are kept separate throughout:

- **[REPO]** verified by reading this repository at `4da0c6b`.
- **[EXP]** verified experimentally, on this machine, with a throwaway probe.
  Every such claim has a measurement behind it.
- **[DESIGN]** a recommendation. Not verified, by definition.

## Summary

The goal is reachable, and the attractive end state described in the request is
the right one: boot on the existing VBE path, promote to virtio-gpu once the
kernel is up, then track host display changes.

The three things worth knowing before reading further:

1. **The host-resize notification path exists and works.** A host window resize
   produces `VIRTIO_GPU_EVENT_DISPLAY`, and `GET_DISPLAY_INFO` then returns
   exactly the new dimensions. Verified for four consecutive resizes, growing
   and shrinking. **[EXP]**
2. **The handoff works, and it is one-way-ish.** VBE displays, then
   `SET_SCANOUT` switches the display to a virtio-gpu resource. Going *back* to
   VBE needs a **full virtio device reset** first; disabling the scanout alone,
   or re-running the VBE mode-set alone, leaves the screen black. **[EXP]**
3. **The infrastructure gap is larger than the video work.** MentOS has no
   virtio code, no PCI capability traversal, no MSI, no scatter/gather type, no
   kernel threads, no workqueues, and its spinlocks do not mask interrupts.
   Phase 2 is mostly a driver-infrastructure project with a console change on
   top. **[REPO]**

The single most consequential constraint is not in the video layer at all: there
is **no process-context deferred-work facility**, so the resize cannot be
performed where the event arrives. Section 4.6 is the load-bearing part of this
document.

---

# Part 1 — What the repository actually provides

All **[REPO]**, verified at `4da0c6b`.

| Facility | Status | Where |
|---|---|---|
| PCI config read/write 8/16/32 | **yes** | `pci_read_8/16/32`, `pci_write_8/16/32` |
| PCI enumeration by class | **yes** | `pci_scan(f, type, extra)`, type = `(class<<16) | (subclass<<8) | prog_if` |
| PCI BAR reading | **yes** | `PCI_BASE_ADDRESS_0..5`; the VBE backend already validates and uses BAR0 |
| **PCI capability traversal** | **NO** | only `#define PCI_CAPABILITY_LIST 0x34` and the status bit exist; no walker |
| **MSI / MSI-X** | **NO** | no code at all |
| Legacy INTx routing | **yes** | `pci_get_interrupt()` reads `PCI_INTERRUPT_LINE` |
| Interrupt registration | **yes**, shared | `irq_install_handler/irq_uninstall_handler`; per-IRQ `shared_interrupt_handlers[]` list |
| Physical page allocation | **yes** | `alloc_pages(gfp, order)` / `free_pages`; `MAX_BUDDYSYSTEM_GFP_ORDER 12` → **16 MiB max contiguous** |
| Page → physical / virtual | **yes** | `get_physical_address_from_page`, `get_virtual_address_from_page` |
| Fixed kernel virtual mapping | **yes** | `mem_upd_vm_area(pgd, virt, phys, size, flags)` — what `vbe_lfb.c` uses |
| Dynamic kernel virtual mapping | **yes** | `vmem_map_physical_pages(page, pfn_count)`, area `0xE8000000-0xEFFFFFFF` (128 MiB) |
| Cache attributes on mappings | **NO** | `MM_CACHE_DISABLE`/`MM_WRITE_THROUGH` exist in the enum but `__set_pg_table_flags()` ignores them |
| kmalloc / slab caches | **yes** | `kmalloc`, `kfree`, `kmem_cache_create/alloc/free` |
| Wait queues | **yes** | `process/wait.h`: `sleep_on`, `sleep_on_interruptible`, `wake_up_all` |
| DMA-visible memory | **yes**, by hand | `alloc_pages(GFP_DMA, order)` + `get_physical_address_from_page`; only example is `ata_dma_alloc()` |
| **Scatter/gather representation** | **NO** | nothing generic; `ata.c`'s `prdt_t` is the only S/G-shaped struct |
| **Virtqueue / virtio** | **NO** | zero virtio code in the tree |
| Spinlocks | **yes**, but | `spinlock_t` is an `atomic_t`; `spinlock_lock()` **does not disable interrupts** |
| Interrupt masking | **yes** | `klib/irqflags.h`: `irq_disable()` returns the previous IF, `irq_enable(flags)` restores it |
| **Kernel threads** | **NO** | no `kernel_thread`, no `kthread_create` |
| **Workqueues / tasklets / softirq** | **NO** | `run_timer_softirq()` is not a work facility — it is the dynamic-timer list, called from inside `timer_handler`, i.e. **interrupt context** |

### Consequences that shape everything downstream

- **`spinlock_lock()` cannot guard state shared with an interrupt handler.** It
  does not mask interrupts, so on this uniprocessor kernel taking it in process
  context and then re-entering from an ISR self-deadlocks. The correct primitive
  for console state shared with an interrupt-context `printf` is
  `irq_disable()/irq_enable()`.
- **There is nowhere to defer work to.** No kernel threads, no workqueues, and
  dynamic timers run in interrupt context. Any allocation or virtqueue wait must
  therefore run on a path that is already in process context. The only such
  paths that touch the console are `procv_write()` and `procv_read()`
  (`kernel/src/io/proc_video.c:227` and `:50`), both reached from a syscall.
- **A single 8 MiB contiguous allocation is possible but unwise.** Order 11 is
  within the buddy limit, but fragmentation makes it fragile. Since virtio-gpu
  accepts a multi-entry backing list (verified below), the framebuffer does not
  need to be contiguous at all.
- **The console has no lock today.** `video.c` has no synchronization whatsoever;
  correctness currently rests on the console being a single flat state machine
  that nothing re-enters. A resize breaks that assumption and is the first thing
  that ever needed serialization.

---

# Part 2 — What QEMU and virtio-gpu actually do

All **[EXP]**, measured with standalone 32-bit multiboot probes (paging off, so
virtual == physical and any static buffer is directly usable as device memory).

## 2.1 The device under `-vga virtio`

PCI `1af4:1050` at `00:02.0`, revision 1, **class 03/00** — so it is a
VGA-compatible controller, which is why the Phase 1 VBE backend's class-based
discovery already finds it and works on it.

| | value |
|---|---|
| `INTERRUPT_PIN` / `INTERRUPT_LINE` | 1 (INTA) / **10** |
| MSI-X capability | present, 3 vectors, **disabled** |
| BAR0 | `0xFE000000`, 8 MiB — the VGA/VBE framebuffer (what Phase 1 uses) |
| BAR2 | `0xFE800000` — the virtio structures |
| BAR4 | `0xFEBF0000` — the VGA MMIO window |
| `COMMON_CFG` | BAR2 + `0x1000` |
| `ISR_CFG` | BAR2 + `0x1800` |
| `DEVICE_CFG` | BAR2 + `0x2000` |
| `NOTIFY_CFG` | BAR2 + `0x3000`, `notify_off_multiplier` = 4 |
| device features | `VIRTIO_GPU_F_EDID`, `VIRTIO_F_VERSION_1`, `VIRTIO_F_RING_RESET` |
| features **not** offered | `VIRGL`, `RESOURCE_BLOB`, `RESOURCE_UUID`, `ACCESS_PLATFORM`, `RING_PACKED` |
| queues | 2 — controlq size 64, cursorq size 16 |
| `num_scanouts` / `num_capsets` | 1 / 0 |

Four facts here matter more than the rest:

1. **Legacy INTx is available and MSI-X is off**, so MentOS needs no MSI work —
   `irq_install_handler(10, ...)` on the existing shared-IRQ list is enough.
2. **The virtio structures are in BAR2, the VBE framebuffer in BAR0.** They are
   different apertures on the same device, which is what makes a handoff
   possible at all: bringing virtio up does not disturb the VBE framebuffer.
3. **`ACCESS_PLATFORM` is not offered**, so there is no IOMMU translation —
   guest physical addresses are what the device wants, and ordinary lowmem pages
   work. No need for `GFP_DMA` (that zone exists for the legacy IDE controller).
4. **This is a modern (virtio 1.0) device only.** Capability-based discovery is
   mandatory, which is why the missing PCI capability walker is milestone zero.

`-device virtio-gpu-pci` is the same device with class 03/80 and the virtio
structures in BAR4; everything else is identical.

## 2.2 Bring-up sequence — verified working

Reset → `ACKNOWLEDGE` → `DRIVER` → write driver features (`VIRTIO_F_VERSION_1`
only) → `FEATURES_OK` (verify it stuck) → select queue 0, publish
`queue_desc`/`queue_driver`/`queue_device`, `queue_enable = 1` → `DRIVER_OK`.
Final `device_status` reads `0x0F`. A plain split virtqueue with a 2-descriptor
chain (one device-readable request, one device-writable response) works.

## 2.3 Commands — verified working

Every one returned its expected response:

| command | response |
|---|---|
| `GET_DISPLAY_INFO` (0x0100) | `RESP_OK_DISPLAY_INFO`, `pmodes[0] = 1280x800 enabled=1` |
| `RESOURCE_CREATE_2D` (0x0101), format `B8G8R8X8_UNORM` (2) | `RESP_OK_NODATA` |
| `RESOURCE_ATTACH_BACKING` (0x0106), **2 non-contiguous entries** | `RESP_OK_NODATA` |
| `TRANSFER_TO_HOST_2D` (0x0105) | `RESP_OK_NODATA` |
| `SET_SCANOUT` (0x0103) | `RESP_OK_NODATA` |
| `RESOURCE_FLUSH` (0x0104) | `RESP_OK_NODATA` |

**Scatter/gather backing works.** The probe backed a 1024x768x32 resource with
two chunks at `0x02000000` and `0x04000000` — physically separate — filled the
first with green and the second with red, and the captured screen showed green
over red with a white marker line exactly at scanline 192. So the framebuffer
does not have to be contiguous, and the stride is simply `width * 4`.

**`B8G8R8X8_UNORM` means a little-endian `uint32_t` of `0x00RRGGBB`.** Writing
`0x0000C000` produced RGB `(0,192,0)`. That is the natural format for the
backend to compose pixels in.

The default `1280x800` is not a host property — it is the device's own
`xres`/`yres`, the same values the stdvga EDID reports. Do not read it as
"the host window is 1280x800".

## 2.4 Host resize → guest, verified end to end

One VNC connection issuing four `SetDesktopSize` requests, with the guest
polling `events_read` and re-issuing `GET_DISPLAY_INFO`:

```
[DISPLAY_INFO #0] pmode0 1280x800  enabled=1   (initial, = device xres/yres)
[EVENT] events_read=0x00000001  (VIRTIO_GPU_EVENT_DISPLAY=1)
[DISPLAY_INFO #1] pmode0 1024x600  enabled=1   (was 1280x800)
[EVENT] events_read=0x00000001
[DISPLAY_INFO #2] pmode0 1600x900  enabled=1   (was 1024x600)
[EVENT] events_read=0x00000001
[DISPLAY_INFO #3] pmode0 1920x1080 enabled=1   (was 1600x900)
[EVENT] events_read=0x00000001
[DISPLAY_INFO #4] pmode0 800x480   enabled=1   (was 1920x1080)
```

Exactly one event per host change, and the reported dimensions match the request
every time, growing and shrinking. This is the mechanism the whole feature needs
and it behaves.

Two details with design consequences:

- **The event carries no dimensions.** It is a config-change interrupt that sets
  bit 0 of `events_read`; the driver must acknowledge by writing `events_clear`
  and then issue `GET_DISPLAY_INFO` to learn the size.
- **Events can burst.** A windowed `-display gtk` run produced *three* events
  before the first successful `GET_DISPLAY_INFO`. So events must be coalesced
  into "geometry is stale, re-read it", never treated as one-event-one-size.

## 2.5 The QEMU UI setting that decides scale-vs-resize

| configuration | event? | guest dimensions change? |
|---|---|---|
| `-vga std` (Bochs VBE), any UI | no | **no** — EDID is static, generated once from `xres`/`yres` |
| `-vga virtio`, `-display gtk` (default) | yes | **yes** |
| `-vga virtio`, `-display gtk,zoom-to-fit=on -full-screen` | yes | **no** — QEMU scales instead |
| `-vga virtio`, `-display sdl -full-screen` | yes | no (in this environment) |

`zoom-to-fit` is precisely the switch between "scale the pixels" — today's
complaint — and "tell the guest to grow". This is worth putting in the user
documentation, because a correct guest implementation still looks broken with
`zoom-to-fit=on`.

I also confirmed the stdvga EDID is property-driven, not host-driven: it reports
1280x800 by default and 1600x1200 with `-device VGA,xres=1600,yres=1200`, and
never changes at runtime. **So Bochs VBE offers no usable resize notification.
Option A of the original comparison is a dead end for dynamic sizing** — which
is exactly why it remains the boot path and nothing more.

## 2.6 The handoff, and the way back

Measured with a probe that programmed VBE 1024x768x8, drew a 16-step grey ramp,
then brought up virtio-gpu and promoted:

| stage | what the captured screen showed |
|---|---|
| VBE active | grey ramp — the 8bpp indexed framebuffer via BAR0 |
| after `SET_SCANOUT(resource 1)` + `FLUSH` | the virtio resource content |
| after `SET_SCANOUT(scanout 0, resource 0)` | **black** |
| after also re-running the VBE mode-set | **still black** |
| after **full virtio device reset** (`device_status = 0`) then the VBE mode-set | grey ramp — **VBE restored** |

So:

- Promotion is safe and atomic from the display's point of view: the VBE image
  stays up until `SET_SCANOUT` succeeds.
- **Demotion is possible but requires a full virtio device reset**, not merely
  disabling the scanout. Any failure-path design that says "fall back to VBE"
  must reset the virtio device first. This is the most surprising finding in the
  investigation and it directly shapes Section 4.8.

## 2.7 What is needed for each of the two goals

The request asks these to be separated, and they differ substantially.

**Merely getting one virtio-gpu framebuffer on screen** needs: PCI capability
traversal; BAR2 mapped; the status/feature handshake with `VIRTIO_F_VERSION_1`;
**one** virtqueue (controlq) with **polled** completion; `RESOURCE_CREATE_2D`,
`RESOURCE_ATTACH_BACKING`, `TRANSFER_TO_HOST_2D`, `SET_SCANOUT`,
`RESOURCE_FLUSH`. It needs **no interrupts**, no cursor queue, no
`GET_DISPLAY_INFO`, and no change to the console's geometry. That is milestone
M3, and it is a self-contained driver exercise.

**Dynamically responding to host display-size changes** adds: the legacy INTx
handler and ISR read; `events_read`/`events_clear` handling and event
coalescing; `GET_DISPLAY_INFO`; clamping policy; a process-context path to do
the work in; and — the large part — a runtime-sized console. That is M5 plus M6,
and the console work is bigger than the driver work.

Nothing in the first tier requires anything from the second. That is what makes
the sequence in Part 6 safe to stop halfway.

---

# Part 3 — The central question, answered

> When I maximize/fullscreen the QEMU window, what guest-visible event occurs
> and what dimensions does the guest receive?

**With `-vga virtio` and a UI that resizes rather than scales:** the guest
receives a virtio config-change interrupt with `VIRTIO_GPU_EVENT_DISPLAY` set in
`events_read`, and a subsequent `GET_DISPLAY_INFO` returns `pmodes[0]` equal to
the new host drawing-area size in pixels. Verified for 1024x600, 1600x900,
1920x1080 and 800x480. **[EXP]**

**With `-vga std`:** nothing. No event, no dimension change, ever. The guest
cannot learn that the window moved. **[EXP]**

**Three things that are not equivalent**, and the design must not conflate them:

1. *Host window pixels* — what the user dragged.
2. *Guest scanout dimensions* — what `GET_DISPLAY_INFO` reports. Equal to (1)
   only when the UI is in resize mode; with `zoom-to-fit=on` it stays put while
   (1) changes.
3. *EDID preferred mode* — for stdvga a static device property; for virtio-gpu
   available via `GET_EDID` under `VIRTIO_GPU_F_EDID` (offered), but not needed
   for a single 2D terminal scanout and not investigated further.

**Unresolved:** the exact numbers under a real fullscreen GTK window on this
WSLg host were **not reproducible**. One run reported 1440x2560 — the transpose
of the 2560x1440 monitor — and later identical runs reported no change at all; a
windowed run reported 640x480. The *mechanism* is proven by the VNC results; the
*specific geometry a fullscreen GTK window yields* needs confirming on a native
X11 or Wayland host before any policy is built on it. See Section 5.

---

# Part 4 — Architecture

## 4.1 Boot backend versus active backend

**The current model cannot express the handoff.** `video.c` calls
`video_backend.*` directly on a single `const video_backend_t` chosen at compile
time, and CMake asserts exactly one such symbol is linked. The handoff needs two
materializations alive at once: VBE displaying while virtio is built.

**[DESIGN]** Keep the compile-time symbol as the **boot** backend and add a
runtime **active** pointer:

```c
/* video.c */
static const video_backend_t *video_active = &video_backend;
```

Every generic call goes through `video_active`. Promotion is then a single
operation:

```c
int video_promote_backend(const video_backend_t *next);
```

which calls `next->late_init()`, and **only on success** publishes
`video_active = next` and repaints. On failure nothing changes and the previous
backend is still displaying.

This is deliberately the same shape as the `late_init` hook Phase 1 already
added — `video_late_init()` is just promotion of the boot backend to itself. The
Phase 1 hook turns out to have been the right abstraction, and reusing it keeps
the concept count down.

The build rule changes from "exactly one backend source" to "exactly one **boot**
backend, plus zero or more promotable ones". `VIDEO_TYPE=VIRTIO_GPU` would
compile `vbe_lfb.c` as boot plus `virtio_gpu.c` as promotable. The CMake
assertion becomes a count of *boot* backends; promotable backends define a
differently-named symbol (e.g. `virtio_gpu_backend`) so duplicate-definition
protection is unchanged.

**Cost:** one pointer indirection per backend call. Against 310 cycles for
drawing a cell, immeasurable.

## 4.2 The VBE → virtio-gpu handoff

**[DESIGN]** Exactly the sequence in the request, which the experiments confirm
is achievable:

1. VBE keeps displaying. Untouched throughout.
2. Bring up virtio: map BAR2, walk capabilities, reset, negotiate
   `VIRTIO_F_VERSION_1`, set up the control queue, `DRIVER_OK`.
3. `GET_DISPLAY_INFO` to learn the host's preferred size. Clamp it (4.8).
4. Allocate the pixel framebuffer as a **list of buddy blocks** and build the
   `mem_entry` array.
5. `RESOURCE_CREATE_2D` → `RESOURCE_ATTACH_BACKING`.
6. Render the current generic console into the new framebuffer **off-screen** —
   the generic cell buffer is the source of truth, so this is just the backend's
   own `put_cells` over the whole grid, into memory nothing is scanning out yet.
7. `TRANSFER_TO_HOST_2D` → `SET_SCANOUT` → `RESOURCE_FLUSH`.
8. **Only now** `video_active = &virtio_gpu_backend`.

Any failure at steps 2-7 unwinds (free what was allocated, reset the virtio
device) and returns non-zero; `video_active` never moved, so the VBE console is
untouched and the boot continues with a working display.

Step 6 is the reason the handoff is content-correct rather than merely
functional: it repaints from the shadow, so the promoted display shows the same
console the user was already looking at, not a blank screen.

**Do not free the VBE mapping on promotion.** It costs one page table and 4 MiB
of address space, and keeping it is what makes the demotion path in 4.8
available.

## 4.3 Runtime console geometry

Today `VIDEO_COLUMNS`/`VIDEO_ROWS` are compile-time macros and `screen`,
`history`, `original_page` are static arrays sized from them (`video.c:107-113`).
**[REPO]**

### Option A — static boot console, dynamic runtime console *(recommended)*

Group the console's storage and shape into one object and make the pointer to it
swappable:

```c
typedef struct {
    unsigned      columns, rows;
    video_cell_t *screen;         /* columns * rows + 1 guard cell */
    video_cell_t *history;        /* STORED_PAGES * rows * columns */
    video_cell_t *original_page;  /* columns * rows */
} console_t;
```

The boot console points at the existing static arrays with the compile-time
geometry — byte-for-byte what happens today. A resize builds a *complete* new
`console_t` from the heap, copies content in, swaps the pointer, and frees the
old one (never freeing the static boot storage).

| criterion | assessment |
|---|---|
| complexity | Moderate. Roughly 40 sites in `video.c` change from a macro to a struct field. Mechanical, but it touches nearly every function. |
| early panic safety | **Best possible.** Before the first resize, storage is static and no allocator is involved, so early boot and the pre-`video_init()` panic path are unchanged by construction. |
| ownership / lifetime | One owner (`video.c`), one object, one free point. The static boot console is never freed — a flag or a pointer comparison distinguishes it. |
| allocation failure | Build-then-swap means failure is a no-op: log and keep the current console. Nothing is ever half-resized. |
| fixed backends | Untouched. With `set_geometry == NULL` the generic layer refuses resizes, so `vga_text.c` and `vga_graphics.c` keep the compile-time console forever. |
| generic `video.c` change | Large but shallow: macro → field, plus one migration function. |

### Option B — compile-time `VIDEO_MAX_*` arrays

Rejected by the request, and independently on merit: it reintroduces an
arbitrary geometry ceiling, and a ceiling generous enough for a 4K window
(480x135 cells) costs 1.55 MiB of permanently resident BSS to serve a console
that usually needs 144 KiB.

### Option C — allocate the console at `video_late_init()`, always dynamic

Does not work. Output exists from the first `printf` in `kmain`, long before any
allocator, so static storage is needed regardless — and then both paths exist
anyway, which is Option A with extra steps.

### Option D — static visible grid, dynamic scrollback only

Genuinely simpler and worth recording. The visible grid is the *small* part
(6144 cells = 12 KiB at 128x48; 16080 cells = 32 KiB at 240x67); the 10-page
scrollback is 10x that. A static visible grid at a modest cap plus a dynamic
history means **the visible grid never moves**, so the interrupt-off critical
section shrinks to a few stores and no copy.

The cost is a compile-time ceiling on visible geometry, which is what the
request rules out. **[DESIGN]** Recommend Option A, and keep D on file as the
fallback if A's migration proves fiddlier than expected — the two differ only in
where the visible grid lives, so switching later is contained.

### All four side by side

| | A: static boot → dynamic runtime | B: `VIDEO_MAX_*` arrays | C: always dynamic | D: static grid, dynamic scrollback |
|---|---|---|---|---|
| complexity | moderate: ~40 mechanical sites + one migration function | **lowest**: geometry becomes two variables, storage never moves | same as A plus a second storage path | low: only `history` migrates |
| early panic safety | **best**: identical to today until the first resize | best: no allocation ever | **broken**: output exists before any allocator | best |
| ownership / lifetime | one owner, one object, one free point; boot storage never freed | nothing to own | two owners during the transition | one owner for history only |
| allocation failure | build-then-swap → no-op, keep current console | cannot fail | fails during boot, which is fatal | no-op, keep current scrollback |
| fixed backends | untouched (`set_geometry == NULL`) | untouched | forced onto the dynamic path for no benefit | untouched |
| `video.c` change | large but shallow | **smallest** | largest | small |
| geometry ceiling | none | **arbitrary compile-time cap** | none | compile-time cap on the *visible* grid only |
| resident cost | 144 KiB static + exactly what is needed | 1.55 MiB static for a 4K-capable cap | 144 KiB static (unavoidable) + dynamic | ~64 KiB static + dynamic history |

B is rejected by the request and independently by the ceiling and the resident
cost. C does not work. A is recommended; D is the fallback.

## 4.4 Resize semantics

**[DESIGN]** Conservative, matching the stated bias, with one deviation
explained below.

**No text reflow. This is not a "keep it simple" call, it is a representational
one.** `screen` and `history` are flat cell arrays with no soft-wrap marker
(`video.c:107-110`) **[REPO]**, so there is *no record* of which rows were
continuations of a logical line. Reflow is therefore not "more code" — it is not
expressible without adding per-row wrap flags and changing the write path, which
is a terminal-emulator change. Explicitly out of scope for Phase 2.

| aspect | behaviour |
|---|---|
| visible cells | Preserved at their cell coordinates, **bottom-anchored**. |
| grow height | New blank rows appear at the **bottom**. Scrollback is *not* pulled forward — history stays history. |
| shrink height | Rows are removed from the **top** and pushed into scrollback, oldest-first, exactly as an ordinary scroll would. This preserves the prompt, which is the content the user is looking at. |
| grow width | Existing cells keep their column; new right-hand columns are blank. |
| shrink width | Right-hand columns are **clipped and lost**. Documented, not silent. |
| cursor | Preserved in cell coordinates, then clamped to the new bounds. A cursor parked on the guard cell is re-parked (quirk 5 preserved). |
| saved cursor (`ESC[s`) | Clamped the same way. |
| scrollback content | **Re-strided**: each stored row is clipped on shrink, blank-padded on grow. Capacity becomes `STORED_PAGES * new_rows`; on shrink the oldest rows are dropped. |
| `scrolled_lines` | **Snapped to the live view before resizing.** If the user is scrolled back, restore the live screen first (existing code path), then resize. Migrating a scrolled-back state plus its `original_page` snapshot across a geometry change is fiddly and the case is rare. Documented as "a resize returns you to the live view". |
| escape parser state | Preserved. `escape_index` and `escape_buffer` are geometry-independent. |
| current colour (`color`) | Preserved. Geometry-independent. |
| cursor overlay / blink | Backend-owned. The backend drops its overlay (marks it not drawn) before the geometry changes; the generic layer re-places the cursor after the repaint — the same order `video_late_init()` already uses. |
| output during a resize | See 4.6. Output before the swap lands in the old buffer and is carried across by the copy; the copy and the swap happen together with interrupts masked, so nothing is lost or duplicated. |

**The one deviation from "purely conservative":** bottom-anchoring on shrink,
rather than clipping the bottom. Clipping the bottom would throw away the prompt
and the last few lines of output — the only part of the screen anyone is looking
at — and would be far more surprising than losing the top rows, which is what
every real terminal does and what the console's own scroll already does.

## 4.5 Backend interface changes

**[DESIGN]** Two additions, both optional, plus one generic entry point. Fixed
backends leave the new fields `NULL` and are entirely unaffected.

```c
/* video_backend_t */

/// Reconfigure for a new cell geometry. NULL = this backend cannot resize.
/// Called from process context, before the generic layer repaints.
int (*set_geometry)(unsigned columns, unsigned rows);
```

```c
/* io/video.h */

/// Record that the display geometry should change. Safe from interrupt context;
/// performs nothing itself.
void video_request_resize(unsigned columns, unsigned rows);

/// Apply any pending resize. Must be called from process context only.
void video_service_pending(void);
```

Answering the specific questions:

- **Should `columns`/`rows` stop being const backend fields?** No — they stay as
  the backend's *boot* geometry, still cross-checked against the geometry header
  at `video_init()`. The authoritative *runtime* geometry moves into the generic
  layer's `console_t`. This keeps the existing consistency check and keeps fixed
  backends declarative.
- **Query or notify?** **Notify.** The event is asynchronous and interrupt-driven;
  polling would mean a timer and a wasted wakeup forever.
- **Who initiates?** The backend *requests* (it owns the font, so it converts
  pixels to cells — that is what keeps pixels out of `video.c`). The generic
  layer *decides and performs*, including clamping and refusal.
- **Can a resize happen in interrupt context?** No. `video_request_resize()` may
  be called from one; applying it may not.
- **Where does allocation/migration happen?** In `video_service_pending()`, from
  process context. See 4.6.
- **How do fixed backends stay simple?** `set_geometry == NULL` means the generic
  layer refuses every resize request. `vga_text.c` and `vga_graphics.c` need no
  edit at all.
- **Is virtio visible in `video.c`?** No. The widest concept crossing the
  boundary is a pair of cell counts.

## 4.6 Concurrency and execution context — the crux

The facts that constrain this are all **[REPO]**: the virtio config-change
interrupt arrives in interrupt context; resizing needs `kmalloc`/`alloc_pages`
and blocking virtqueue completions; there are no kernel threads and no
workqueues; dynamic timers run in interrupt context; and `spinlock_lock()` does
not mask interrupts.

**[DESIGN]** Split the work by context:

**In the virtio interrupt handler** — read the ISR register (which acknowledges
it), and if the config-change bit is set: write `events_clear`, set a
"geometry is stale" flag, and `wake_up_all()` the console wait queue. Nothing
else. No allocation, no virtqueue traffic, no `GET_DISPLAY_INFO` (it would have
to wait for a completion).

**In process context** — `video_service_pending()` does the real work: issue
`GET_DISPLAY_INFO`, clamp, allocate, migrate, `set_geometry`, repaint.

**Where is it called from?** The two console paths that are certainly process
context: `procv_write()` (`proc_video.c:227`) and `procv_read()`
(`proc_video.c:50`). Waking the console wait queue from the interrupt is what
makes this prompt rather than lazy: a shell blocked in `procv_read()` wakes,
services the resize, and only then continues reading. **[REPO]** confirms
`procv_read()` already blocks via `keyboard_wait()` at `proc_video.c:93`, so the
wake path exists.

**Honest limitation:** with no kernel thread, a resize is serviced at the next
console read or write. In practice the shell is blocked in `procv_read()` at the
prompt, so the wake makes it near-immediate. If the guest is genuinely doing
nothing that touches the console, the resize applies when it next does. This is
a real constraint of the current kernel, it should be documented rather than
papered over, and the upgrade path (a kernel thread, or a proper workqueue) is
obvious and independent.

**Serialization against output.** Today `video.c` has no lock at all, and
`printf` reaches it from interrupt context. **[REPO]** Adding a spinlock would
be wrong (it does not mask interrupts). Instead:

- Everything expensive — `GET_DISPLAY_INFO`, allocation, building the new
  `console_t`, `set_geometry` — runs **outside** any critical section, on
  buffers nothing else can see. The old console stays live and keeps serving
  output throughout.
- Only the **content copy and the pointer swap** run with interrupts masked, via
  `irq_disable()`/`irq_enable()` from `klib/irqflags.h`. The copy must be inside
  the window, not before it, or output arriving between copy and swap would be
  lost.
- The repaint runs after the swap, with interrupts enabled.

The masked window is one bounded copy: 385 KiB at 240x67, well under a
millisecond. Acceptable for a rare user-initiated event, and worth stating in
the documentation as a known interrupt-latency spike.

## 4.7 Framebuffer representation and memory cost

**[DESIGN]** Use **32 bpp `B8G8R8X8_UNORM`** for virtio-gpu. Do not carry the
8bpp indexed model over: virtio-gpu has no palette concept, 32bpp is what QEMU
wants natively, and the conversion is trivial — the backend keeps a 16-entry
`uint32_t` table built from the shared `video_palette_16` and indexes it with the
attribute nibbles. The generic console keeps its 4-bit fg/bg attributes; pixel
format stays entirely inside the backend, exactly as now.

A cell scan line becomes 8 `uint32_t` stores instead of 2 — four times the
traffic of the VBE backend, but into ordinary RAM. Phase 1 measured 310
cycles/cell at 8bpp; expect roughly 4x that, which is still an order of
magnitude better than the planar backend's ~13.7 kcycles.

Cell state is 2 bytes per cell across three buffers: `screen`
(`columns*rows + 1`), `history` (`STORED_PAGES=10` screens), and `original_page`
(one screen) — so `24 * columns * rows + 2` bytes in total, of which the
scrollback is ten twelfths.

| mode | cells | screen | scrollback | original_page | cell state total | framebuffer (32bpp) | wasted pixels |
|---|---|---|---|---|---|---|---|
| 1024x768 | 128x48 = 6144 | 12.0 KiB | 120 KiB | 12.0 KiB | **144 KiB** | 3.0 MiB | none (both divide exactly) |
| 1280x720 | 160x45 = 7200 | 14.1 KiB | 141 KiB | 14.1 KiB | **169 KiB** | 3.5 MiB | none |
| 1920x1080 | 240x67 = 16080 | 31.4 KiB | 314 KiB | 31.4 KiB | **377 KiB** | 7.9 MiB | 8 scan lines (1080/16 = 67.5) |

The scrollback dominating the cell state is what makes Option D in 4.3 tempting
and is worth remembering if the resident cost ever becomes a problem: capping
`STORED_PAGES` is a far cheaper lever than capping geometry.

virtio metadata, controlq of 64: descriptors 1024 B, avail 134 B, used 518 B —
under one page if packed together. The cursor queue (16 entries, ~428 B) is
**not needed initially**: the console draws its own cursor into the framebuffer,
exactly as both graphical backends already do, so the hardware cursor plane can
wait indefinitely.

The `mem_entry` list is 16 B per entry, so its size depends entirely on
allocation granularity: 7.9 MiB as single pages is 2025 entries (32 KiB), but as
two order-10 (4 MiB) buddy blocks it is 32 bytes. **[DESIGN]** Allocate
largest-block-first — try successively smaller orders and append each block as
one `mem_entry`. This keeps the descriptor tiny, avoids depending on one big
order-11 allocation succeeding, and degrades gracefully under fragmentation.

Worst case total at 1920x1080: ~7.9 MiB framebuffer + 377 KiB cell state + ~12
KiB virtio metadata ≈ **8.3 MiB**, against 896 MiB of low memory.

## 4.8 Failure model

**[DESIGN]** The governing rule: **VBE remains the fallback for every failure
before `SET_SCANOUT` succeeds.** After that, 2.6 shows the way back requires a
full virtio device reset, so post-promotion failures are handled *within* virtio
by keeping the last good scanout, and demotion is reserved for a device that is
genuinely dead.

| failure | behaviour |
|---|---|
| no virtio-gpu device | Promotion never attempted. VBE console, unchanged. Not an error — this is the `-vga std` case and must stay silent beyond a notice. |
| PCI capability walk finds no `COMMON_CFG` | Not a modern virtio device. Abort promotion, VBE stays. |
| `VIRTIO_F_VERSION_1` not offered, or `FEATURES_OK` rejected | Abort, reset the device, VBE stays. |
| virtqueue allocation failure | Free what was taken, reset the device, VBE stays. |
| framebuffer allocation failure | Free the queues, reset, VBE stays. |
| BAR2 mapping failure | Abort before touching the device, VBE stays. |
| any command returns an error response | Abort, unwind, reset, VBE stays. |
| absurd / unsupported dimensions | Clamp before allocating: floor at 80x25 cells so the boot log's `width - 5` status column stays valid, ceiling by a byte budget on framebuffer plus cell state. Outside the range → keep the current geometry and log; do **not** tear anything down. |
| resize allocation failure while virtio is active | Keep the current geometry and the current scanout. The display keeps working at the old size; the host letterboxes. Log once. **Never** free the working framebuffer before the replacement is fully built. |
| device signals `NEEDS_RESET` | Demote: full virtio device reset, then re-program the VBE mode and repaint from the shadow. Verified to restore the display (2.6). Requires the VBE mapping to have been retained (4.2). |
| host reports a size implying 0 rows or 0 columns | Treat as "display off": ignore, keep the current geometry. |

Every unwind path is the same shape and should be one function, so there is
exactly one place where "give up and leave VBE alone" is implemented.

---

# Part 5 — Unresolved questions

1. **What geometry does a real fullscreen GTK window yield?** The mechanism is
   proven, the numbers are not. One WSLg run reported 1440x2560 (transposed
   relative to the 2560x1440 monitor), later identical runs reported no change,
   and a windowed run reported 640x480. Needs a native X11 or Wayland host.
   Until then, clamping (4.8) must not assume landscape.
2. **What is the actual event latency**, and is `wake_up_all()` on the console
   queue enough to make servicing feel immediate? Measurable once M6 exists.
3. **Is IRQ 10 unmasked** by `pic8259_init_irq()`, and does the shared-handler
   list behave with a second device on the line? Cheap to check during M6.
4. **VNC `SetDesktopSize` returned status 4** for virtio-gpu (and 3 for stdvga),
   which is not an RFB-standard code, yet the resize took effect. Cosmetic — it
   affects only the test harness, not the guest — but worth understanding before
   relying on the VNC path in automated tests.
5. **Does `GET_EDID` add anything** over `GET_DISPLAY_INFO` for a single 2D
   scanout? `VIRTIO_GPU_F_EDID` is offered. Assumed no; not investigated,
   deliberately.
6. **Should `TRANSFER_TO_HOST_2D` be per-damage-rectangle or whole-screen?** The
   probe transferred the whole 1024x768 resource each time. A terminal changes a
   line at a time, so per-rectangle transfer is the obvious optimisation, but it
   should be measured rather than assumed — Phase 1 already showed that
   intuitions about where the cost lies here are unreliable.

---

# Part 6 — Proposed commit sequence

Derived from the gap table in Part 1. Each milestone is independently reviewable
and leaves a working tree.

| # | commit | why here |
|---|---|---|
| **M0** | `feat(pci): add capability-list traversal` | Modern virtio discovery is impossible without it, and it is a self-contained ~60-line addition to `pci.c` with no dependency on anything else. Verifiable on its own by logging the capability chain of `00:02.0` and comparing against the values in 2.1. |
| **M1** | `feat(virtio): minimal modern virtio-pci transport` | Device-independent: capability discovery, BAR mapping via `mem_upd_vm_area`, the status handshake, feature negotiation, one split virtqueue, notify, **polled** completion. No interrupts yet — polling first keeps M1 testable without touching the IRQ path. Ships with no user. |
| **M2** | `feat(video): make the active backend a runtime choice` | Introduces `video_active` and `video_promote_backend()`; `video_late_init()` becomes promotion of the boot backend. Pure refactor — all three existing backends must stay pixel-identical. |
| **M3** | `feat(video): add a virtio-gpu 2D backend at fixed geometry` | Resource create / attach / scanout / transfer / flush at **128x48, unchanged**. This is the "one framebuffer on screen" milestone: the console never learns anything happened. Promotion from VBE included, with the full unwind path from 4.8. |
| **M4** | `refactor(video): make console storage runtime-sized` | The `console_t` indirection from 4.3, boot console still static, **no resize yet**. The gate is that every backend's output stays pixel-identical to `4da0c6b`. Deliberately separate from M5 so a large mechanical diff is reviewed without behaviour mixed in. |
| **M5** | `feat(video): support runtime geometry change` | `video_request_resize()`, `video_service_pending()`, migration, and the 4.4 semantics. Driven by a **synthetic trigger** (a `/proc/video` write, say) so resize is fully testable *before* any virtio event exists. This ordering is the point: it decouples the hardest console work from the hardest driver work. |
| **M6** | `feat(virtio-gpu): react to display-change events` | Legacy INTx on IRQ 10 via `irq_install_handler`, ISR read, `events_clear`, coalescing, `GET_DISPLAY_INFO`, clamp, `video_request_resize()`. First point at which the original feature is user-visible. |
| **M7** | `docs(video)` + characterization additions | Maintainer documentation, the resize semantics table, the QEMU `zoom-to-fit` note, and the verification recipes below. |

M0-M1 are infrastructure with no video content; M4-M5 are console work with no
virtio content; only M3 and M6 touch both. That separation is intentional — it
keeps each review focused on one subsystem.

---

# Part 7 — Verification plan

Existing gates that must keep passing at **every** milestone:

- VGA text screendump **pixel-identical** to the `d31df9b` baseline (720x400).
- Planar VGA screendump pixel-identical (640x480).
- VBE screendump 1024x768, 128x48, boot log legible, `[OK]` at column 123.
- `Kernel tests completed: 16/16` on every backend.
- `make qemu-test`: 52 tests, 0 failures, 0 panics, on every backend.
- Release builds clean.
- Remember the pre-existing ~3% `Cannot find the superblock` mount flake
  (documented in `docs/maintainer/video-backends.md`) before blaming a change.

New tests, by milestone:

| test | how | from |
|---|---|---|
| VBE-only boot unaffected | Existing screendump gates under `-vga std` | M3 |
| VBE under `-vga virtio` still a viable fallback | Boot with `-vga virtio`, no virtio promotion compiled in; expect the 1024x768 console (already verified working at `3592274`) | M3 |
| virtio init failure leaves VBE visible | Fault injection: force each unwind point in 4.8 to fail in turn; screendump must still show the VBE console and the boot must complete | M3 |
| handoff is content-correct | Screendump immediately before and after promotion; decode both to text with the font-based decoder; the character grids must be **identical** | M3 |
| no allocation or virtio dependency in early console paths | Boot with a deliberately broken virtio device (`-vga std`); assert the boot log is complete and unchanged. Plus a grep-level check that `video.c` calls no allocator before the first resize | M4 |
| repeated grow/shrink cycles | Synthetic trigger: 200 alternating geometry changes; assert content and cursor consistency each time | M5 |
| memory leaks across repeated resizing | `ENABLE_KMEM_TRACE`/`ENABLE_PAGE_TRACE` are already build options; free-page count before and after 200 cycles must match | M5 |
| resize while output is occurring | Drive continuous output while triggering resizes; assert no lost or duplicated lines and no panic | M5 |
| cursor after resize | Exactly one solid cell at the expected position, blink still alternating — the Phase 1 frame-hash method | M5 |
| scrollback before/after resize | Fill scrollback with numbered lines, resize, page back; assert the numbers are still consecutive and correctly clipped | M5 |
| resize semantics table | One assertion per row of the 4.4 table, in the characterization suite where it can use the public API | M5 |
| host resize end to end | The VNC `SetDesktopSize` harness from 2.4, driving 1024x600 / 1600x900 / 1920x1080 / 800x480; assert the console geometry follows and the screen stays legible | M6 |
| fullscreen under the project's real UI | `-display gtk -full-screen`; **manual** until question 1 in Part 5 is resolved | M6 |
| event coalescing | Burst several resizes faster than servicing; assert exactly one migration and the final geometry correct | M6 |

Three harness pieces built during this investigation are worth keeping, since
they turn "looks right" into an assertion:

1. **Font-based screendump decoder** — matches each 8x16 cell's foreground mask
   against `video_font_8x16.h` and recovers the text exactly. Already used for
   the Phase 1 scroll verification.
2. **RFB `SetDesktopSize` client** — drives host-side resizes with no GUI and no
   window manager, which is what makes resize testing automatable at all.
3. **Standalone multiboot virtio probe** — a paging-off harness where virtual ==
   physical, so device sequences can be tried without any kernel infrastructure.
   This is how every fact in Part 2 was established, and it is the cheapest way
   to answer the next protocol question too.

One trap it cost time to find, worth recording for whoever writes M1: in a
probe like this, **the used-ring index must be read through a volatile pointer**.
At `-O2` gcc hoists a plain `used.idx` read out of the poll loop, which presents
as a device that answers every request exactly one request late. The same
applies to the real driver.
