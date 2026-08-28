# Video backends

Verified against `42cf8a1`. Core files: `kernel/src/io/video.c` (generic),
`kernel/inc/io/video_backend.h` (interface), `kernel/src/io/video/` (backends).
Shared assets: `kernel/src/io/video_font.c` with
`kernel/inc/io/video/video_font.h` (the bitmap fonts) and `video_palette_16.h`
(the 16 console colours), both used by every backend that draws text as pixels.

## Split of responsibility

The console is two layers.

**Generic (`video.c`)** owns all terminal state: the cell contents, the cursor,
the ANSI escape parser, the colour tables and the scrollback. It works purely in
cell coordinates and has no reference to an address, a memory layout, a screen
dimension or an I/O port.

**Backend** knows only how to put cells on a display. It is write-only and holds
no terminal state.

The generic layer's cell buffer is the **source of truth**. Every mutation must
be followed by a `put_cells()` covering the range that changed; a mutation
without a flush makes the display drift out of sync with the buffer. This is the
one invariant to respect when editing `video.c`.

## Interface

`video_backend_t` (`kernel/inc/io/video_backend.h`): `init`, `late_init`,
`put_cells`, `scroll`, `set_cursor_position`, `set_cursor_style`,
`set_geometry`, `request_font`, `service`, `cursor_blink`, plus
`name`/`columns`/`rows`.

The last five are all optional. A backend that leaves them NULL — both fixed
backends do — cannot resize, cannot change font, has nothing to do in process
context, and has a hardware cursor. Nothing about the runtime-geometry machinery
costs it anything.

- `put_cells(column, row, cells, count)` writes cells in row-major order,
  wrapping at `columns` and ignoring anything past the last row. Because the
  generic layer always updates its buffer first, this doubles as the universal
  "flush this range" call — which is why there is deliberately **no** separate
  fill, clear or draw-character operation.
- `scroll(rows)` moves displayed content vertically, positive up. It exists
  because this is the one operation a backend can do far more cheaply than a
  cell-by-cell repaint — the text backend does a `memmove`, and the graphical
  one moves the display window and copies nothing at all. Uncovered rows are
  left undefined; the caller always repaints them.
- `set_cursor_position(column, row, cell)` carries the cell the cursor now
  sits on. A backend that draws its own cursor needs it to be able to take the
  cursor away again — see the cursor lifecycle below.
- `set_cursor_style` takes a **semantic** style: a shape (`HIDDEN`, `BLOCK`,
  `UNDERLINE`, `BAR`) plus a `blinking` flag, not scan lines, so a backend with
  no hardware cursor can implement it by drawing. `HIDDEN` is currently
  unreachable through ANSI. Blink is carried separately because a hardware
  cursor blinks on its own and cannot be told not to, while a drawn one blinks
  only if something toggles it.
- `cursor_blink` is **optional** (NULL for a hardware cursor). The PIT handler
  calls `video_cursor_blink_tick()`, which forwards to it. The rate is the
  backend's business; this is just the tick.
- `late_init` is **optional** (NULL unless the backend has something it cannot
  do at `video_init()` time). See the two-stage lifecycle below.
- `set_geometry(columns, rows)` is **optional**; NULL means the console refuses
  every resize. Its currency is **cells**, never pixels: a backend owns its font
  and derives the pixel geometry itself. That is deliberate, and it is what would
  make a future font-size change the same operation as a display change rather
  than a new mechanism. It must be transactional — prepare, do not switch.
- `request_font(reset, steps)` is **optional**; NULL means the console drops
  font requests where they are made. It owns the whole transition: choose the
  font, work out the cell counts its scanout now divides into, and call
  `video_change_geometry()`. A step count rather than a direction, so that two
  requests arriving before a service cannot collapse into one. See the runtime
  font section below.
- `service` is **optional**; called from `video_service_pending()` in process
  context, for work a backend noticed in an interrupt but could not do there.

`video_cell_t.attribute` is `foreground | (background << 4)`, 4 bits each, in
IBM CGA/VGA order. This is the console's colour model, shared by all backends,
which is what keeps the ANSI colour tables in `video.c` backend-independent. A
backend in a different colour space translates inside its own `put_cells`.

## Selection

Compile-time, via the `VIDEO_TYPE` CMake cache variable (same shape as
`SCHEDULER_TYPE`/`KEYMAP_TYPE`). Resolved in `kernel/CMakeLists.txt` **before**
the source glob, because it must both pick the backend source and name its
geometry header.

| `VIDEO_TYPE` | Source | Geometry |
|---|---|---|
| `VGA_TEXT_MODE` (default) | `src/io/video/vga_text.c` | 80x25 |
| `VGA_MODE_640_480_16` | `src/io/video/vga_graphics.c` | 80x30 |
| `VBE_MODE_1024_768_8` | `src/io/video/vbe_lfb.c` | 128x48 |
| `VIRTIO_GPU` | boot on `vbe_lfb.c`, promote to `virtio_gpu.c` | 128x48, then whatever the host asks for |

Everything under `src/io/video/` is filtered out of the glob and only the
selected source is added back: every backend defines the same `video_backend`
symbol, so two reaching the link is a duplicate definition, not a silent
fallback. The filter covers the **whole directory** rather than a file-name
pattern -- it used to match `vga_*.c`, which meant a backend named anything else
was linked *in addition to* the selected one. After the filter and the append,
CMake asserts that exactly one backend source is in the list, so "exactly one"
is a property of the build and not of the naming. A `VIDEO_TYPE` in the list
without a branch mapping it to a source and a geometry header now fails the
configure step instead of the link.

Geometry is a compile-time property of the selected backend. CMake passes
`-DVIDEO_GEOMETRY_HEADER=...`; `video_backend.h` includes it and `#error`s if
`VIDEO_COLUMNS`/`VIDEO_ROWS` are missing. The generic layer sizes its static
state from those macros and the backend initializes its reported `columns`/`rows`
from the same ones, so the two cannot drift. Nothing generic hardcodes a
dimension.

Static console storage is `boot_screen` + `boot_history` + `boot_original_page`:
48002 bytes at 80x25, 57602 at 80x30, 147458 at 128x48 (measured in the object
file). That is the **boot** console; once resized, storage is allocated and sized
from the runtime geometry. The formula is `24 * columns * rows + 2`, of which the
scrollback is ten twelfths — so capping `STORED_PAGES` is a far cheaper lever
than capping geometry if the resident cost ever matters.

### Adding a backend

1. Add `kernel/inc/io/video/<name>_geometry.h` defining `VIDEO_COLUMNS` and
   `VIDEO_ROWS`.
2. Add `kernel/src/io/video/<name>.c` defining `video_backend`. Any name works;
   the filter excludes the whole directory.
3. Add the name to `VIDEO_TYPES` and a branch mapping it to its source and
   geometry header. Forgetting the branch is now a configure error.

## Early boot — the constraint that shapes all of this

`video_init()` is called at `kernel.c:153`, **before** `pmmngr_init`,
`kmem_cache_init`, `paging_init` and `vmem_init`. A backend may therefore use
**static storage only**: no allocation of any kind, and no dependency on paging,
the timer, locks or the scheduler.

`printf` -> `video_puts` is reachable **before** `video_init()`, on the
invalid-multiboot-magic path, which then panics and never reaches
`video_init()`. Console output must not depend on initialization having run. The
`video_backend` symbol is statically initialized const data precisely so that it
is already valid at that point: no lazy init, no allocation, nothing that can
fail.

`paging.c:119` identity-maps the first 1 MB, so `0xB8000` and the `0xA0000`
graphics window stay reachable before and after paging is enabled.

Anything **outside** that first megabyte is not reachable at `video_init()` time
and cannot be made reachable there. The bootloader's page directory identity-maps
only the 896 MB of low memory `boot/src/boot.c` admits, there is no allocator to
build a mapping with, and `paging_init()` later switches to a page directory of
its own that would drop a hand-built mapping anyway. A backend for such hardware
needs the two-stage lifecycle below.

`kernel_panic` itself writes only to serial via `pr_emerg`; it reaches video
indirectly, through the `printf`/`print_fail()` that usually precedes it.

### Pre-initialization contract

`put_cells`, `scroll` and the cursor operations **may be called before
`init()`**. A backend whose hardware is not usable at reset must ignore those
calls.

This gate belongs to the backend, never to the generic layer. The VGA text
adapter works straight out of reset, so gating writes generically would silence
the early diagnostic on the invalid-multiboot path.

There is no replay of pre-init content: `video_init()` calls `backend.init()` and
then `video_clear()`, and that clear is what publishes the initialized console.
Anything printed earlier is discarded, which is what has always happened.

**Documented limitation:** in a graphical build, a panic before `video_init()`
leaves nothing on screen, because the mode was never programmed. The serial log
still has it.

### Two-stage lifecycle

`video_late_init()` is called from `kmain()` immediately after `paging_init()`
succeeds, and forwards to the backend's optional `late_init`. It exists for
hardware that the section above puts out of reach at `video_init()` time.

By then `mem_upd_vm_area()` and the page-table cache are available, so a backend
can map a PCI BAR into kernel virtual space -- which is exactly what the VBE
backend does.

The window between the two calls spans about nine boot steps, and a deferring
backend must be **completely inert** across it: no device memory, no device
registers, nothing. The generic layer keeps recording into its cell buffer, and
on success `video_late_init()` flushes the whole buffer and places the cursor. So
nothing is lost -- the display comes up showing every line printed since
`video_init()`, early ones included, not just the ones after the mapping.

`video_late_init()` prints **only to serial**. Do not make it print to the
console: the VGA text pixel baseline is a verification gate and an extra line
would break it.

**Documented limitation:** for a deferring backend, a panic anywhere between
`video_init()` and `video_late_init()` reaches serial only. This is the same
trade the graphical backends already make before `video_init()`, extended by
those nine steps.

## VGA text backend

`0xB8000`, two bytes per cell, 80x25. A VGA text cell is a character byte
followed by an attribute byte, which is exactly `video_cell_t`, so the
framebuffer is addressed directly with no conversion (guarded by a compile-time
size check). `put_cells` is a `memcpy`, `scroll` a `memmove`. Cursor position and
shape go to the CRTC registers at `0x3D4`/`0x3D5`.

Cursor styles map to scan-line ranges: `BLOCK` `(0,15)`, `UNDERLINE` `(13,15)`,
`BAR` `(0,1)`, `HIDDEN` via bit 5 of the cursor-start register.

`BAR` is worth knowing about: scan lines 0-1 render as a thin **horizontal**
sliver across the top of the cell. VGA text mode cannot draw a vertical bar at
all, so the historical `// vertical bar cursor` comment described an intent the
hardware never delivered. The range is preserved verbatim to keep text mode
rendering unchanged; the graphical backend draws a real vertical bar.

## Graphical VGA backend

640x480, 16 colours, planar, 8x16 font, exactly 80x30 cells (both divisions
exact). Four one-bit-per-pixel planes overlaid at `0xA0000`; the four bits of a
pixel index a 16-entry palette, which maps straight onto the attribute byte, so
no colour translation is needed.

The 8-pixel font width is the reason this is cheap: a glyph scan line is exactly
one byte per plane at a byte-aligned address, so a cell is drawn with plain byte
stores — no read-modify-write, no per-pixel loop. Per plane, a scan line is the
glyph byte where the plane's foreground bit is set and its complement where the
background bit is, so when the two bits are equal the whole cell is one constant
in that plane.

### Performance, and why it is shaped this way

Every access to the graphics window traps into the emulated adapter and costs
roughly **580 cycles regardless of width** (measured: 153600 byte writes and the
same bytes as 38400 dword writes differ by ~3x in total, not in per-access
cost). So the only two levers are how many accesses are made and how many are
avoided outright. Both are used:

- `__vga_draw_run` is **plane-major**: each plane is selected once per run
  rather than four times per cell, turning 4N port accesses into 4.
- Within a plane it is **line-major**, which makes a scan line's destination
  bytes contiguous so four cells go out in one aligned 32-bit access. Writes are
  aligned first; an unaligned access is split back apart and gives the saving
  away.
- Scrolling makes **no memory accesses at all** (see below).

Plane-major on its own was worth only ~20%, because port I/O was never the
bottleneck. Measure before optimising here; the intuition that port writes
dominate is wrong.

| Operation | Before | After |
|---|---|---|
| Full screen repaint | 110 Mcycles | 33 Mcycles |
| One character typed | 3.3 Mcycles | 1.1 Mcycles |
| One scroll | 64 Mcycles | 1.5 kcycles |

Note the console flushes **from the cursor to the end of the line** on every
character, because writing inserts. That is a property of the generic layer, not
something the backend may narrow.

### Scrolling: the display window, not the pixels

Video memory is a ring of `VGA_VIRTUAL_ROWS` = 51 text rows (65280 bytes, the
largest whole number of rows inside the 64 KB window the CPU can address), of
which 30 are visible. Scrolling moves `start_row`, and two CRTC registers follow:

- the **start address** picks the first byte displayed;
- the **line compare** closes the ring, being the scan line at which the display
  abandons the sequential address and restarts from address 0.

This is what fixes tearing, not just what makes scrolling fast. Copying four
planes one after another while the display scans them means that, for the
duration of the copy, a pixel's four colour bits come from different scroll
generations — which is exactly the coloured, duplicated text that used to
appear. Moving the window is a register change the display picks up between
frames, so no intermediate state is ever shown.

Blanking or vertical-retrace synchronisation could **not** have fixed this: the
copy took about 18 ms against a vertical blanking interval of roughly 1.4 ms, so
there was never a window to hide it in.

### Verified register semantics — do not re-derive from documentation

These were established by probing the target, and two of them contradict what
the documentation would lead you to expect:

1. **The start address is in bytes.** 80 shifts the display exactly one scan
   line; 1280 exactly one text row. A value of 40 produces no clean shift, which
   rules out word addressing.
2. **The address counter does not wrap at 64 KB.** Past the end of the buffer
   the display reads zeros, so a plain circular buffer is not available. This is
   why line compare is needed at all.
3. **Line compare provides the wrap.** With the window at 28160 and the compare
   at 464, the display shows virtual rows 22 to 50 and then restarts at address
   0, exactly as predicted.
4. **The split must be parked at the maximum the field can hold when unused.**
   Any value past the last scan line ought to be equally inert. It is not: at an
   intermediate value the display truncates rather than ignoring it. This cost a
   boot-time regression to find — the screen froze at 19 rows while the kernel
   ran on happily — so `VGA_LINE_COMPARE_OFF` means 1023 and nothing else.

The two line-compare bits that share registers with vertical timings are rebuilt
from the mode table rather than read back, so a scroll can only ever disturb the
bit it owns.

### Cursor lifecycle — the invariant

**This section applies to both graphical backends.** `vbe_lfb.c` implements the
same lifecycle with the same four pieces of state; only the drawing differs.

A software cursor is pixels drawn over a cell, so it has to be taken away again.
Getting that wrong produces ghosts, trails and vanishing cursors, all of which
were once separate-looking bugs with one cause. The invariant:

1. The generic cell buffer **never** contains cursor pixels.
2. The overlay exists only in video memory, and only transiently.
3. It is removed by **restoring the cell underneath it**.
4. Only the code that drew it removes it.

That last point is why erasing belongs to the **backend**, not the generic
layer. The generic layer used to repaint the cell the cursor had vacated, which
cannot work: `scroll()` moves the overlay's pixels along with the content, so
after a scroll the generic layer repaints a cell the overlay is no longer on and
leaves a block behind where it moved to. **That was the ghost.**

The backend therefore keeps four things and nothing more — position, the cell
underneath, the style, and a flag saying whether an overlay is currently drawn:

| Event | What happens |
|---|---|
| `set_cursor_position` | hide → adopt position and cell → show |
| `set_cursor_style` | hide → adopt style, reset blink phase → show |
| `put_cells` covering the cursor cell | update cached cell → render the run (which wipes the overlay) → show |
| `scroll` | the overlay moved with the content, so follow it; if it left the screen, mark it not drawn |
| blink tick | show or hide according to the new phase |

`hide` and `show` are both idempotent and both gated on the drawn flag, which is
what keeps them from getting out of step.

Two details worth keeping:

- The cell is **passed in**, not looked up. A drawn glyph is not reversible, and
  keeping a copy of the screen to look it up in would duplicate the generic
  layer's state. One cell is enough, so one cell is cached.
- The overlay takes the cell's own foreground colour, as the hardware cursor
  does — but a cell erased to attribute 0 would give a black block on black.
  When foreground and background match, the overlay falls back to the default
  foreground. **That was the cursor vanishing after backspace and delete.**

### Blinking

The console has no periodic source of its own. Dynamic timers (`add_timer`) are
`kmalloc`'d and freed by the timer subsystem, so re-arming one per toggle would
mean a heap allocation several times a second forever. The PIT handler is the
only other periodic source, so `timer_handler` calls
`video_cursor_blink_tick()` — the single change this required outside the
backend. All blink logic and the rate stay in the backend, which counts the
ticks it is handed.

Because it is timer-driven, the cursor blinks while the machine is idle. Do not
replace this with a toggle driven by keyboard or video activity.

Geometry is derived from register values, not comments — CRTC `0x13` (offset)
`= 0x28 = 40` words `= 80` bytes per scan line per plane `= 640` pixels, and
CRTC `0x12 = 0xDF` extended by the `0x07 = 0x3E` overflow bits `= 479`, so 480
lines. A compile-time check ties the geometry header to the mode and the font.

### Defects fixed while adapting the reference implementation

Do not reintroduce these when porting more legacy modes:

1. **DAC components must be scaled 8-bit -> 6-bit.** The DAC takes 6 bits, so
   writing 8-bit values drops the top two and programs `{128,0,0}` as pure
   **black**. Shift right by two.
2. **The attribute-controller palette must be the identity mapping** when only
   16 DAC entries are loaded. The EGA-compatible mapping the reference used
   (`{0,1,2,3,4,5,0x14,7,0x38,...}`) sends attribute indices 6 and 8-15 to DAC
   entries `0x14` and `0x38`-`0x3F`, which such a load never initialises.
3. **The glyph blitter was off by one column** (`x + (width - cx)` instead of
   `x + (width - 1 - cx)`), and the string helper advanced a fixed 8 pixels
   regardless of font width. Neither survives the byte-aligned formulation used
   here.
4. Palette and mode tables were non-`static` definitions in headers relying on
   `-fcommon`; they are `static const` now. The mode-setting code also mutated
   its own table to force the CRTC unlock bits — the register values already
   satisfy them, and the unlock is done on the hardware instead.

**The comments in the reference material are not reliable.** Verified wrong:
`// 80x60` on the 640x480 mode (its 8x14 font gives 34 rows), `// 40x25` on
320x200 (its 5x6 font gives 64x33), `// 90x60` on 720x480 (gives 90x30), and
`__write_font` reads the font *out of* video memory despite its name. Derive
geometry from mode registers and font dimensions.

## VBE linear-framebuffer backend

1024x768, 8 bits per pixel, 8x16 font, exactly 128x48 cells (both divisions
exact). This is the backend that answers the original request: the same font on a
larger mode shows **more** of the terminal, rather than showing the same 80x30
terminal scaled up to fill the window.

One byte per pixel indexes the DAC, and the console's attribute nibbles are
already palette indices, so no colour translation is needed. It loads the same
`video_palette_16` entries as the planar backend, which is what makes the
histogram cross-check below meaningful.

### The device, and how it is found

The Bochs VBE (DISPI) extension, which QEMU's `-vga std` -- what this project
already launches with -- provides, as do `-vga virtio` and `-device
bochs-display`. Verified on QEMU 8.2.2:

| | `-vga std` | `-vga virtio` |
|---|---|---|
| PCI id | `1234:1111` at `00:02.0` | `1af4:1050` at `00:02.0` |
| DISPI revision | `0xB0C5` | `0xB0C5` |
| framebuffer (BAR0) | `0xFD000000`, 16 MiB | `0xFE000000`, 8 MiB |

Discovery matches on the PCI **class** (3/0/0, a VGA-compatible controller) and
not on a vendor/device pair, which is why one backend covers all three devices.
The class alone is not sufficient -- `cirrus-vga` is the same class and has none
of these registers -- so the **DISPI interface revision is the real gate**, and
it is checked first.

The mode is programmed through the legacy I/O ports `0x01CE`/`0x01CF`
deliberately. The same registers are also available through an MMIO window in
BAR2, which would need a mapping; the ports do not, and that is what lets
discovery happen at `video_init()` time.

`-vga std` survives `-nodefaults` — verified, the device is still at `00:02.0`.

### Performance — it is faster than the planar backend, not slower

The framebuffer is a plain RAM region behind a PCI BAR (`info mtree` shows
`vga.vram (prio 1, ram)`), **not** a trapped aperture like `0xA0000`. So none of
the trap-avoidance shape the planar backend needs applies here. Measured on the
target:

| Operation | planar 640x480 (80x30) | VBE 1024x768 (128x48) |
|---|---|---|
| Full repaint | 33 Mcycles | 2.0 Mcycles |
| One character typed | 1.1 Mcycles | 0.085 Mcycles |
| One cell, warm | ~13.7 kcycles | 310 cycles |
| One scroll | 1.5 kcycles | 787 cycles |

For contrast, the same 32 dword stores through the `0xA0000` window cost 45730
cycles. That two-orders-of-magnitude gap is why banking the framebuffer through
the low window -- the one approach that *would* work before paging -- was
rejected.

A cell's scan line is eight consecutive bytes at an eight-byte-aligned address,
so it is two aligned 32-bit stores selected between a replicated foreground and
background byte, with no read-modify-write and no per-pixel loop. `vbe_nibble_mask`
does the selection; its values are written out rather than computed because the
highest bit of a glyph nibble has to select the **lowest** byte of the word, and
getting that backwards mirrors every glyph.

The run loop is line-major purely for locality — one pass writes one scan line
across the whole run, walking forward through consecutive addresses. There are no
plane selects to amortise, so this is a cache concern, not a port-I/O one.

### Scrolling: panning, and the rebase at both ends

Video memory is treated as 256 text rows (4 MiB, which is the mapped window), of
which 48 are visible. Scrolling moves `start_row` and reprograms one register,
the DISPI vertical pan, so it copies nothing.

Unlike the planar backend there is **no line compare and no wrap**: the display
reads forward from the pan offset and past the end of video memory it would read
whatever is there. So `start_row` is kept inside `[0, 256-48]` and, when a scroll
would leave that range, `__vbe_rebase()` copies the rows that are still going to
be visible to the opposite end and the pan restarts there.

**Both ends are reachable and both are handled.** Forward scrolling runs off the
far end; paging back through scrollback runs off the near end just as readily,
and the generic layer will do 480 backward scrolls without pausing. A rebase
happens once per 208 scrolls in one direction and costs one copy of at most a
screenful, about 1.3 Mcycles.

A rebase cannot tear, and the reason is worth keeping. Nothing is copied out of
the region the display is currently reading, and nothing is copied into it, and
the pan is reprogrammed only once the copy is finished. That disjointness is what
`vbe_ring_headroom_check` guarantees, and it is why the buffer is **three**
screens rather than merely two: a rebase downwards copies from as high as row
`2*VIDEO_ROWS-2` into a destination starting at `VBE_RING_ROWS-VIDEO_ROWS`, so
two screens would let the two regions meet.

### The BAR is validated, not assumed

`__vbe_bar_address()` rejects an I/O BAR and a 64-bit BAR with a diagnostic
rather than masking and using them, and requires the address to be non-zero and
page aligned. A 64-bit BAR with a zero upper half would happen to work on a
32-bit kernel, which is exactly why it is refused rather than silently accepted.

The discovered physical address and the kernel virtual address are kept
**separate**. The mapping goes at `VBE_FB_VIRT_BASE` = `0xF9000000`, so where the
firmware puts the BAR does not move it. That window is free by construction: the
kernel's linear map starts at `0xC0000000` and covers at most 896 MB, so it
cannot reach `0xF8000000`, which is where the linker script's unused
`KERNEL_HIGHMEM` region begins; `vmem`'s area is `0xE8000000`-`0xEFFFFFFF`, below
it. `vbe_virt_window_check` pins both ends of the window against those facts, and
the upper end against the I/O APIC at `0xFEC00000`.

### Failure mode, and limitations

If discovery fails the backend stays inert for the whole boot and the console is
serial-only. There is no fallback to text mode, and there cannot be: the geometry
is compile-time 128x48 and the text adapter is 80x25. The diagnostics say which
check failed.

The mapping is write-back cacheable. `mem_upd_vm_area()` has no way to ask for
anything else — `MM_CACHE_DISABLE` exists in the flag enum but
`__set_pg_table_flags()` does not implement it — and under QEMU the region is
coherent with the guest's view regardless. Real hardware would want
write-combining here.

## Runtime geometry

The console's shape is a runtime property. `video_columns`, `video_rows` and the
three buffer pointers in `video.c` are variables, not macros and arrays.

Storage starts in the static `boot_*` arrays sized by the geometry header, and it
has to: output exists long before there is an allocator, and the
pre-`video_init()` panic path must not depend on one. A resize allocates
replacement storage, migrates into it, and frees the old — except the boot arrays,
which are static and never freed. `console_pages` being NULL is what distinguishes
the two.

### Who does what, and in which context

This is the part worth understanding before editing any of it, because the split
is forced by what this kernel does not have: no kernel threads, no workqueues,
and `spinlock_lock()` does not mask interrupts, so it cannot guard console state
against an interrupt-context `printf`.

| step | context | may it allocate? |
|---|---|---|
| backend notices a display change | interrupt | **no** — flag it and wake the console's readers |
| `video_request_resize()` | any, including interrupt | **no** — validates and records, last request wins |
| `video_service_pending()` -> `backend->service()` | process | yes — this is where `GET_DISPLAY_INFO` happens |
| allocate new console, `set_geometry()` | process | yes |
| copy + publish | **interrupts masked** | **no** |
| free old storage, repaint | process | yes |

`video_service_pending()` is called from `procv_read()` and `procv_write()`,
which are the console's own syscall paths. `keyboard_wake_readers()` exists so an
interrupt can get a blocked shell moving again, which is what makes servicing
feel immediate at a prompt. **The consequence to be honest about:** a guest that
touches the console not at all leaves a resize pending. Do not fix that by
inventing a worker subsystem inside the video driver.

Interrupts are masked for the copy **and** the publish together, and nothing
else. Not just the publish: output arriving between a copy and a swap would land
in the buffer that had already been read, and would be lost. The masked section
is one bounded `memcpy` of at most `CONSOLE_MAX_BYTES`.

### Limits

Orientation-agnostic, because a host really does report portrait displays.
Per-dimension floors and ceilings plus a byte budget, with both dimensions
checked **before** `columns * rows` is computed so neither the product nor its
multiple can overflow. The 32-column floor is not 80: the boot log's `width - 5`
marker needs 80, but that is a property of the compile-time boot geometry, and by
the time anything can resize the boot log is over.

### Resize semantics

Pinned by measurement, not by intent — verify these from cell contents, not from
cursor coordinates.

| aspect | behaviour |
|---|---|
| grow taller | content stays, new rows blank at the **bottom** |
| shrink | rows drop off the **top** into scrollback |
| grow wider | new columns blank on the right |
| shrink narrower | right-hand columns **clipped and lost** |
| cursor, saved cursor | cell coordinates kept, adjusted for dropped rows, then clamped; a parked cursor stays parked |
| scrollback | re-strided per row, rows that fell off the screen appended as newest, oldest dropped to fit |
| scrolled back during a resize | returns to the live view first |
| escape parser, current colour | untouched; both are geometry-independent |
| cursor overlay | backend drops it; the generic repaint re-places it |

The asymmetry is the point: growing keeps the cursor where the user left it,
shrinking keeps the prompt. Both are what real terminals do.

**There is no reflow, and this is not a simplification.** `screen` and `history`
are flat cell arrays with no soft-wrap marker, so there is no record of which
rows were continuations of a logical line. Reflow is not something this
representation can express; adding it means per-row wrap flags and a changed
write path, which is a terminal-emulator change and not this one.

## Fonts, and runtime font size

### The asset

A font is a `video_font_t` (`kernel/inc/io/video/video_font.h`): a bitmap, the
size it was drawn at, and an integer magnification. The glyph data lives in one
translation unit (`kernel/src/io/video_font.c`) so a build with two graphical
backends links one copy, and the arrays are private — a backend reaches a glyph
through the descriptor, which is what stops the dimensions and the bitmap they
belong to from drifting apart.

That file sits beside `video.c` rather than in `src/io/video/` because the build
filters that whole directory down to the single selected backend. A shared asset
is not a backend, and keeping it out of there is what lets the filter stay a rule
about a directory instead of a list of exceptions.

There are deliberately **two** accessors, and the split is about speed:

| accessor | returns | for |
|---|---|---|
| `video_font_glyph(font, ch)` | `const uint8_t *`, one byte per scan line | backends drawing at scale 1 |
| `video_font_scanline(font, ch, line)` | `uint32_t` mask, bit (width−1) leftmost | backends that magnify |

Both fixed graphical backends use the first, and must keep using it. They turn a
glyph *byte* straight into device words — one byte per plane for the planar
adapter, a nibble-expanded pair of aligned 32-bit stores for the linear one — and
both tricks exist only because a glyph is exactly eight pixels wide. The planar
one has a measured 4x in it (see its performance section). Neither backend can
change font, so neither needs the general path.

`video_font_scanline` divides to find the asset's scan line and repeats each of
its pixels across. Unmagnified it returns the byte unchanged, which is the common
case. A base asset is at most eight pixels wide, because one byte per scan line
is what makes a glyph cheap; a wider native font would be a change to that header
and to nothing else.

### The zoom ladder, and what is not on it

| rung | asset | scale | cell | @1024x768 | @1920x1080 |
|---|---|---|---|---|---|
| 0 (default) | 8x16 | 1 | 8x16 | 128x48 | 240x67 |
| 1 | 8x16 | 2 | 16x32 | 64x24 | 120x33 |
| 2 | 8x16 | 3 | 24x48 | 42x16 | 80x22 |

Font zoom is **proportional**: every step scales both dimensions by the same
factor, so a smaller font exposes more rows *and* more columns. That is the whole
point of it, and it is why the ladder is one asset magnified rather than a
collection of hand-drawn sizes.

It bottoms out at the default because there is nothing to go below it with. A
proportional half-step would be a 4x8 font; this project has never had one, and
decimating the 8x16 would not be legible. Commit `ea8957b` removed five fonts
along with the old VGA graphics drivers, and none of the smaller ones is a
half-step:

| asset | in | aspect (w/h) | why it is not a zoom step |
|---|---|---|---|
| 8x16 | `video_font.c` | 0.50 | this *is* the default |
| 8x14 | `ea8957b^` | 0.57 | same width — buys rows, no columns |
| 8x8 | `video_font.c` | 1.00 | same width — buys rows, no columns |
| 5x6 | `ea8957b^` | 0.83 | smaller in both, but a different aspect; legible, no descenders |
| 4x6 | `ea8957b^` | 0.67 | smaller in both, but barely legible |

8x8 is restored and described but **not selectable**: it belongs to a separate
"compact font" choice, not to zoom. 5x6 is the interesting one if such a choice is
ever added — it genuinely gives more rows and more columns — and the accessor
already handles its narrower packing, since bit (base_width − 1) is the leftmost
pixel at any width.

Steps clamp at the ends of the ladder rather than wrapping or failing, so a held
key settles at the largest or smallest size. A font that is not a rung counts as
the default for stepping.

### Three quantities, kept apart

The virtio-gpu backend tracks the scanout size it is filling (`gpu_target_*`)
separately from the cell counts, because the two move independently:

```
columns = target_width  / font_width
rows    = target_height / font_height
```

- a **display change** moves the target, and the cells are recomputed with the
  current font — so a resize keeps whatever size was chosen;
- a **font change** leaves the target alone, and the cells are recomputed with
  the new font — so the scanout does not move.

Before this split the scanout *was* the cell counts multiplied out, which was
fine with a fixed font and wrong the moment it was not: 1080 scan lines is 67
cells of 16 but only 33 cells of 32, so a font change would have shrunk the
display. The framebuffer now covers the union of the cell area and the target,
which as a side effect means the scanout is exactly the size the host asked for —
a 1024x600 window used to get a 1024x592 scanout, discarding eight scan lines.
Damage is tracked in cell rows and cannot describe the leftover margin, so the
first transfer after a geometry change covers the whole framebuffer.

A sub-cell change in the host window updates the target but does not rebuild, so
the scanout can lag the window by less than one cell until something acts. That
is deliberate: rebuilding on every pixel of a window drag would mean an
allocation, a mapping and a new resource per event.

### The transaction

A font change is a geometry change that the backend drives, because the backend
is the only thing that knows how big a cell is:

1. `video_request_font(direction)` — accumulates. Called from the escape parser,
   which runs wherever `video_putc()` does, so it must not allocate.
2. `video_service_pending()` — process context — calls `backend->service()`, then
   `backend->request_font(reset, steps)`.
3. The backend picks the font from the ladder, divides its target, stages the
   font, and calls `video_change_geometry()`.
4. `set_geometry` builds a framebuffer for the staged font without switching; the
   existing atomic publish swaps framebuffer, resource **and** font together.
5. The staging is cleared whatever the outcome, which is what stops a refused
   font from leaking into the next display resize.

Failure at any point leaves the old font drawing the old geometry on the same
scanout — verified for a refused cell count, an injected console-storage
allocation failure and an injected framebuffer allocation failure, all three
pixel-identical and all three recovering on the next attempt.

### Two orderings that are load-bearing

**Requests accumulate.** The pending state is a signed step count, not the latest
direction. Storing a direction would turn two "larger" into one whenever both
arrived before a service, which is exactly what a repeating key does. A request
for the default is absolute: it clears the count and raises its own flag.

**A font change is serviced instead of a pending resize, not around one.**
`backend->service()` runs first, so the display size is re-queried before
anything divides it; then the font change runs and a pending resize is dropped —
**only if the font change succeeded**. Both would compute their cell counts from
the same scanout, so running both would resize twice and show the intermediate
geometry; and dropping the resize on failure would cost the display its chance to
catch up with the window. A resize requested directly rather than derived from
the scanout (there is no such caller outside tests) loses to the font change.

### Which backends support it

| backend | draws through the abstraction | runtime font change |
|---|---|---|
| VGA text | no — the font is the hardware's | **no** |
| planar VGA 640x480 | yes, scale 1 | no |
| VBE LFB 1024x768 | yes, scale 1 | no |
| virtio-gpu | yes, any scale | **yes** |

VGA text would need the font RAM reprogrammed and the CRTC maximum-scan-line
register changed, which is exactly the pixel equivalence this document makes a
gate. It reports unsupported.

The two fixed graphical backends have fixed modes, so `set_geometry` is NULL and
a font change has nowhere to put the cells. VBE is the one that could plausibly
grow the ability, and the concrete obstacle is worth recording: its scroll ring
is 256 **cell rows**, which at 24x48 is 12288 scan lines against the 8192 that
8 MiB of video memory at 1024 bytes per line provides. The ring size would have
to become font-dependent, and the compile-time non-overlap proof
(`VBE_RING_ROWS >= 3 * VIDEO_ROWS`) would become a runtime one — inside the one
piece of code here that has a proof attached.

### How the user reaches it

Console control in this kernel already had a shape, and font size follows it:

```
F12 -> keyboard driver -> "\033[24~" -> shell -> "\033[2z" -> video.c parser
```

The keyboard turns a key into an escape sequence for userspace, the shell turns
that into a console escape sequence, and the console's parser acts on it. That is
exactly how Page Up already reaches the scrollback — the shell answers
`ESC [ 5 ~` with `ESC [ 25 S`. The keys belong in the shell; the console control
belongs in the console.

`ESC [ <n> z` selects the size: `0` or absent is the default, `1` one step
smaller, `2` one step larger. Private, and legitimately so — ECMA-48 reserves the
final bytes `0x70`–`0x7E` for private use, which is where the existing
cursor-shape `q` already sits.

F10, F11 and F12 are default, smaller and larger. **Not** Ctrl+Plus and
Ctrl+Minus: the keyboard driver emits sequences for the function keys already but
nothing for Ctrl with a punctuation key, so those would need the driver extended
first. Handling F10–F12 in the shell also stopped them leaving a stray `~` in the
line — it read the `2` of `ESC [ 21 ~` as Insert and echoed the rest. The login
prompt has its own parser and still does that; left alone as unrelated.

The console is serviced after a write as well as before it. Otherwise a request
recorded while parsing output waits for the next read, and reads block on a
keypress, so the font would appear to change only when the user next typed
something.

## Virtio-gpu backend

Never the boot backend. Virtio needs PCI capability walking, page allocation and
kernel mappings, none of which exist at `video_init()` time, so the machine boots
on VBE and `virtio_gpu_promote()` hands the console over once everything is up.

32 bpp `B8G8R8X8_UNORM`, which on a little-endian machine is a `uint32_t` of
`0x00RRGGBB`. The palette is **quantised to 6 bits** before being widened back,
reproducing what the VGA DAC does to the same palette in the other two backends —
because "all backends produce the same set of RGB values" is a verification gate
below, and an unquantised 32 bpp backend would fail it while looking right.

### The framebuffer is scattered but looks linear

The device accepts a resource backed by a **list** of physical ranges, so the
framebuffer never has to be one buddy block — 7.9 MiB at 1920x1080 would be a
fragile thing to demand. It is allocated largest-block-first, stepping down an
order when an allocation fails, and each block becomes one memory entry.

The blocks are then mapped **consecutively** into a fixed kernel window, so the
device reads a scattered resource while the drawing code writes one flat array.
Measured block counts: two on a fresh system, up to seven after repeated resizes,
which is why the limit is sixteen.

### The page-directory trap — read this before mapping anything late

`__gpu_reserve_window()` maps the whole 16 MiB window **not-present** at
bring-up, purely to get its page tables allocated. That is not tidiness, it is
required.

`mm_create_blank()` snapshots the main page directory with `memcpy`. Page tables
that exist at that moment are shared, so later changes to page-table *entries*
are visible everywhere — but a page-directory *entry* added afterwards is
invisible to every process already created, and a syscall-serviced resize runs in
exactly such a process.

The symptom is nasty: every `mem_upd_vm_area()` call returns 0 and the access
faults anyway, and it only appears once a mapping crosses into a 4 MiB region
that did not already have a page table. So it hides until a size threshold is
crossed. Tracked as issue #271.

### Two things it deliberately does not do

**No cursor blink.** Making a drawn pixel visible needs a transfer and a flush,
which are device round trips, and those may not happen in the timer interrupt.
`cursor_blink` is NULL and the cursor is steady; it is still drawn, moved and
removed exactly as the planar backend's is.

**No cursor queue.** The console draws its own cursor into the framebuffer, as
both other graphical backends do, so a hardware cursor plane would buy nothing.

### Re-entrancy

`printf` reaches the console from any context, and a polled virtqueue submission
holding one outstanding chain would be corrupted by being re-entered. Every
submission is guarded: a re-entrant caller draws into the framebuffer, records the
damage and returns without touching the device, and the next submission that is
not re-entered picks it up. Damage is a cell-row range, so a keystroke costs one
row's transfer.

### Scrolling

Moves pixels and resends everything, because a 2D resource has no pan this
backend can move cheaply. That makes a scroll the expensive operation in exchange
for a keystroke costing one row — the opposite trade from the VBE backend, and
the right one when the framebuffer is ordinary RAM.

### The retired resource

Released at the start of the *next* resize, not at the switch, because the switch
is reachable from an interrupt and the page allocator is not interrupt-safe. Two
framebuffers are live at once: bounded, not leaked.

### Host display changes

The event path is `config-change interrupt -> events_read/events_clear -> flag ->
GET_DISPLAY_INFO -> cells = pixels / font -> video_request_resize()`. The event
carries **no dimensions**, and several arrive in a burst, so they coalesce into
one "geometry is stale" condition and the device is asked once.

Only virtio-gpu offers this. Bochs VBE has nothing equivalent: its EDID is
generated once from device properties and never changes. That is why VBE remains
the boot path and nothing more.

**The QEMU setting that decides whether any of this is visible:**

| configuration | guest dimensions change? |
|---|---|
| `-vga std`, any UI | no, ever |
| `-vga virtio`, `-display gtk` (default) | **yes** |
| `-vga virtio`, `-display gtk,zoom-to-fit=on` | no — QEMU scales instead |

`zoom-to-fit` is exactly the switch between "scale the pixels" and "tell the
guest to grow". A correct guest still looks broken with it on.

## Preserved quirks — do not "fix" incidentally

These are long-standing behaviours pinned by
`kernel/src/tests/unit/test_video.c`. Changing any of them is a deliberate
decision, not a cleanup.

1. **Erase writes `{0x00, 0x00}`**, not `{' ', 0x07}`: character *and* attribute
   are zeroed.
2. **Erase-to-space uses the current colour.** Backspace and delete write
   `{' ', color}`, which is a different fill from erase. Three distinct fill
   values coexist; do not unify them.
3. **The cursor sync mutates the buffer.** If the cell at the cursor has
   `character == 0` it writes `{' ', color}` there, which is what makes the
   cursor visible over an empty cell.
4. **Writing a character inserts**, shifting the rest of the line right; the
   last cell of the line falls off.
5. **The cursor may rest past the end of the screen.** Moving forward off the
   bottom-right corner parks it there, `video_get_cursor_position` then reports
   `(0,0)`, and the rendered position clamps to `(0, rows-1)`. The old
   implementation kept writing through such a cursor into the unused part of the
   text aperture; the cursor is now clamped onto a single guard cell and those
   invisible writes are skipped, so the out-of-bounds writes are gone while
   everything observable is unchanged.
6. **`ESC [ 0 ; 0 H` lands on the last cell, not home.** The zero parameter
   underflows the one-based conversion and is then caught by the upper clamp.
7. **`ESC [ S` and `ESC [ T` are inverted** with respect to their letters, and
   treat a missing parameter as zero lines rather than one, so a bare `ESC [ S`
   does nothing.
8. **`batch_cursor_updates`** (set by `video_puts`) suppresses the quirk-3
   space materialisation for intermediate characters, so it changes what lands
   in the buffer. It is not a pure optimisation.
9. **`\t` and other control characters are ignored** by `video_putc` without
   moving the cursor. Tab expansion and control-character echo live in
   `proc_video.c`.

## Verifying a change

Two complementary gates. Neither is sufficient alone.

**Characterization suite** — `kernel/src/tests/unit/test_video.c`, registered in
`kernel/src/tests/runner.c`, built with `-DENABLE_KERNEL_TESTS=ON` (off in CI).
Asserts through the **public API only**, so it is backend-independent and the
same assertions validate every backend. Since the public API cannot read screen
content, it pins the cursor and geometry contract only. It is module 16 of 16 in
the runner; the pass line to look for is
`Kernel tests completed: 16/16 passed`.

**Cursor-overlay check** — the lifecycle invariant is a pixel property, so the
public API cannot see it. Drive the console with a probe (type, newline,
backspace, delete, then a run of ordinary characters), screendump, and count
cells that are *solidly* filled: there must be exactly one, at the cursor.
More than one is a ghost or a trail; none, with a steady style, is the vanishing
cursor. To watch blinking, take a dozen dumps ~120 ms apart while idle and
confirm the phase alternates.

Two practical notes on the solid-cell count:

- It only means "exactly one" on a screen with no coloured **backgrounds**. A
  space with a non-black background is solidly filled too, and the shell's
  post-login banner has several. Past the login prompt, look for the one solid
  cell that *moves with typing* and ignore the ones that stay put; on the boot
  screen the plain count works.
- Blink makes the count alternate between one and zero. That alternation is the
  pass condition, not a flake. The whole frame should differ **only** by that
  cell -- hash the frames and confirm there are exactly two distinct hashes,
  which is a stronger statement than counting, because it also proves the
  overlay is removed by restoring exactly the cell underneath.

Credentials for driving an interactive probe are `root`/`root` (the shadow hashes
are SHA-256 of the password repeated 100000 times; see `userspace/bin/login.c`).
Keys go in through the QEMU monitor's `sendkey`.

**Long-scroll check** — boot scrolls far too few times to exercise either
backend's virtual buffer. In a 128x48 build the boot log does not fill the screen
at all, so it never scrolls once.

For the planar backend, print ~150 lines and confirm they come out in order; 120
scrolls laps the 51-row ring more than twice with the split active across its
range.

For the VBE backend the boundary is 208 scrolls **in each direction**, and both
directions must be covered — a forward-only test says nothing about the rebase
that scrollback triggers. Print ~500 lines, then page back ~470, then page
forward ~470 again. That is 4 forward and 2 backward rebases, and the round trip
must land exactly where it started. Verified this way at `3592274`:

| after | screen shows | rebases so far |
|---|---|---|
| 500 lines printed | lines 454-500, rows 0-46, consecutive | 2 forward |
| 470 lines paged back | boot log above, lines 1-31 below, consecutive | 2 backward |
| 470 lines paged forward | lines 454-500 again, identical | 4 forward |

Decoding a screendump back into text makes this checkable rather than
eyeballable: the cells are a fixed grid and `video_font.c` is the only
glyph source, so matching each cell's foreground mask against the font recovers
the characters exactly. At a magnified font, sample the top-left pixel of each
scale x scale block and the base 8x16 bitmap comes straight back, so the same
font table works at every size.

**Do not assume a screendump's rows are `3 * width` bytes.** QEMU pads each PPM
row up to a four-byte boundary, so any width that is not a multiple of four has a
longer stride than the naive one -- at 1366, `1366 * 3` is 4098 and the row is
actually 4100. A decoder that assumes the tight stride shears the image by two
bytes per row, which produces a convincing diagonal smear, blended-looking
colours and glyphs that no longer match the font: it looks exactly like a
rendering bug in the guest. Measure the stride as `payload_size / height`
instead. Every geometry used before the widescreen pass happened to have a
width divisible by four (1024, 1280, 1920, 640, 600, 768), which is why this
never showed up until 1366. Note that codes 0, 32 and 255 share a blank bitmap, so
map an all-zero mask to a space rather than trusting a reverse lookup.

**Pixel-exact screendump** — pins the rendered content the API cannot observe.
Boot headless with a QEMU monitor socket, `screendump`, and `cmp` against a
baseline captured before the change. Independent boots produce byte-identical
captures, so a difference is a real change, not noise. Capture sizes: VGA text
720x400, planar 640x480, VBE 1024x768.

The cheapest way to get a trustworthy baseline is a detached worktree at the
commit before the change (`git worktree add --detach <dir> <rev>`), built and
booted the same way. That leaves the working checkout alone.

```sh
qemu-system-i386 -vga std -m 1096M -nodefaults -serial file:/dev/null \
  -drive file=build/rootfs.img,format=raw,if=ide,index=0,media=disk,snapshot=on \
  -monitor unix:/tmp/mon.sock,server,nowait -display none \
  -kernel build/mentos/bootloader.bin &
sleep 15
printf 'screendump /tmp/shot.ppm\nquit\n' | nc -U /tmp/mon.sock
```

Note the AF_UNIX 108-byte path limit: keep the socket path short.

A useful cross-check on colour: histogram the captures. **All three** backends
must produce exactly the same set of RGB values for the same console output. Any
divergence means a palette or attribute bug. Do it on a post-login screen as well
as on the boot log -- the boot log only uses three colours, where the shell
prompt uses six. Verified at `3592274`: identical sets of 3 and of 6 across VGA
text, planar VGA and VBE.

**Font checks** — everything below was exercised at the font commits, on
virtio-gpu, by sending the escape sequence and decoding the result. A synthetic
trigger is the right way to start, before the escape sequence exists: record a
request and call `video_service_pending()` from a temporary probe.

Cover, at minimum: default -> larger -> larger -> larger (the third must clamp
with no transition at all) -> smaller -> default; a font change at several display
sizes including **portrait**; a host resize after a font change, which must keep
the chosen font; a font change after a host resize; the cursor at the last row and
the last column; a font change while scrolled back; a size the console refuses;
and a long run of changes with the free-page count checked at both ends.

Three things are worth checking specifically, because they are the ones that
would break silently:

- **The scanout must not move.** Decode the capture's *pixel* dimensions, not just
  the cell counts. At 1024x768 every rung produces a 1024x768 capture: 128x48,
  64x24, 42x16.
- **The magnification must be exact.** For every cell, every pixel of each
  scale x scale block must equal its top-left pixel. A single ragged block means
  something is interpolating or misaligned.
- **The cursor scales.** The solid block measures 8x16, 16x32 or 24x48 pixels and
  stays cell-aligned. Find it geometrically — the block is the largest uniform
  non-background rectangle — because a decoder that picks the most common colour
  as the background reads a fully solid cell as blank.

Content preservation is the same bottom-anchor/no-reflow policy as a display
resize, and is checkable exactly: the new screen's rows must equal the old
screen's **last** `rows` rows, each **clipped** to the new column count. Verified
byte-for-byte at 128x48 -> 64x24.

On memory: the free-page count is identical across long runs of changes, but the
first change costs two one-off amounts that are not a leak — the console leaves
its static boot arrays for allocated storage, and one retired framebuffer is held
until the next resize. Measure between two points *after* the first change, or
the first reading will look like a 3.25 MiB leak at 1024x768.

`t_font` in the userspace suite is a smoke test, not a substitute for the above.
Userspace cannot ask how many cells the console has — there is no `TIOCGWINSZ` —
so it asserts only that the sequences are consumed rather than printed and that
the console survives. On a `VIRTIO_GPU` build it does drive four real font changes
mid-suite, and the remaining tests then run at the new geometry, which is the part
worth having.

**Resize checks** — the semantics table above is a pixel property, so drive it
and decode the result. A synthetic trigger is enough and is the right way to
start: `video_request_resize()` followed by `video_service_pending()` from a
temporary probe exercises the whole console path with no display event involved.

Cover, at minimum: grow to a larger landscape geometry; shrink; **portrait**
(rows greater than columns — 60x120 works and has been tested); portrait back to
landscape; repeated grow/shrink cycles; a resize while scrolled back; the cursor
at the last cell of the screen followed by a shrink; and geometries that must be
refused. Verified this way at `294f5cb`:

| step | asked | got | what the cells showed |
|---|---|---|---|
| grow | 160x50 | 1280x800 px | content unmoved, blanks at the bottom |
| shrink | 100x30 | 800x480 px | last 27 lines kept at the top, line ends clipped |
| portrait | 60x120 | 480x1920 px | accepted, content preserved |
| 80 cycles | 150x44 / 96x30 | all correct | 88 resizes, 0 failures, content still consecutive |
| refusals | 10x4, 5000x5000, 1000x1000 | all refused | console unchanged |
| cursor at 127x47, shrink to 64x20 | — | 63x19 | clamped to the new last cell |

Repeated cycling is also the memory-leak check: if resources or framebuffers were
leaking, allocation fails or block counts climb without bound. 88 resizes with a
peak of seven blocks is the signature of steady state.

**Host-resize check** — the end-to-end one, and it needs no GUI. Drive QEMU's VNC
server with a `SetDesktopSize` request; that is the same `dpy_set_ui_info()` path
its GTK window uses when resized. Then send any keypress, so the console's read
path runs and services the pending resize, and screendump. Verified at `294f5cb`
for 1600x900, 1024x600, 1920x1080 and 1280x720 in one session, the console
following each.

Plus `make qemu-test` holding at 52 tests / 0 failures on **each** backend, with
`grep -c PANIC build/serial.log` equal to 0.

`make qemu-test` runs on the display device the configured backend needs: CMake
maps `VIDEO_TYPE` to `-vga std` for the three fixed backends and to `-vga virtio`
for `VIRTIO_GPU`, and passes the same flags to `make qemu`. A `VIRTIO_GPU` kernel
under `-vga std` is the trap this closes — it finds no device, keeps its VBE boot
backend, and passes all 52 tests on the fallback with nothing in the output
saying so. So for a promoting backend the wrapper also **asserts the hand-over**:
CMake tells it which backend the console must end up on, and it fails the run if
the kernel log never says the console got there. Verified both ways at the
harness commit — green on `-vga virtio`, and the same build rejected with exit 1
on `-vga std` despite QEMU exiting 33 and all 52 tests passing.

Driving a host resize is deliberately *not* in the wrapper: `-nographic` means
there is no UI to resize, and the RFB recipe below needs a VNC client that CI has
no reason to grow. Under `-display none` the device reports its property default
(1280x800 on QEMU 8.2.2) to `GET_DISPLAY_INFO` and never raises a display-change
event, so the automated run covers promotion and steady-state drawing, and the
resize path stays a manual check.

Two traps worth knowing, both of which look like a regression and are not:

1. QEMU **writes to `rootfs.img`**, so repeated boots eventually corrupt it and
   the guest panics mounting ext2 with an ATA `PIO failed` error. That is the
   image, not your change. `make qemu-test` opens the disk with `snapshot=on`, so
   it cannot happen there; `make qemu` still writes, and `make -C build
   filesystem` regenerates the image.
2. There is a **pre-existing intermittent mount failure** of roughly 3%:
   `Cannot find the superblock (/dev/hda)` followed by a panic, with a clean
   `e2fsck`-verified image and `snapshot=on`. Measured at 1/34 boots on `d31df9b`
   and 1/32 on `3592274`, so the rate is unchanged. Before blaming a change for
   it, reproduce it on a pristine baseline; a single failure in a handful of boots
   is not evidence of anything.
