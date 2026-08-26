# Video backends

Verified against `999efd7`. Core files: `kernel/src/io/video.c` (generic),
`kernel/inc/io/video_backend.h` (interface), `kernel/src/io/video/` (backends).

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

`video_backend_t` (`kernel/inc/io/video_backend.h`): `init`, `put_cells`,
`scroll`, `set_cursor_position`, `set_cursor_style`, plus `name`/`columns`/`rows`.

- `put_cells(column, row, cells, count)` writes cells in row-major order,
  wrapping at `columns` and ignoring anything past the last row. Because the
  generic layer always updates its buffer first, this doubles as the universal
  "flush this range" call — which is why there is deliberately **no** separate
  fill, clear or draw-character operation.
- `scroll(rows)` moves displayed content vertically, positive up. It exists
  because this is the one operation a backend can do far more cheaply than a
  cell-by-cell repaint: a `memmove` in video memory instead of thousands of
  glyphs. Uncovered rows are left undefined; the caller always repaints them.
- `set_cursor_style` takes a **semantic** style (`HIDDEN`, `BLOCK`,
  `UNDERLINE`, `BAR`), not scan lines, so a backend with no hardware cursor can
  implement it by drawing. `HIDDEN` is currently unreachable through ANSI.

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

All backend sources are filtered out of the glob and only the selected one is
added back: every backend defines the same `video_backend` symbol, so two
reaching the link is a duplicate definition, not a silent fallback.

Geometry is a compile-time property of the selected backend. CMake passes
`-DVIDEO_GEOMETRY_HEADER=...`; `video_backend.h` includes it and `#error`s if
`VIDEO_COLUMNS`/`VIDEO_ROWS` are missing. The generic layer sizes its static
state from those macros and the backend initializes its reported `columns`/`rows`
from the same ones, so the two cannot drift. Nothing generic hardcodes a
dimension.

Static console storage is `screen` + `history` + `original_page`: 48002 bytes at
80x25, 57602 at 80x30.

### Adding a backend

1. Add `kernel/inc/io/video/<name>_geometry.h` defining `VIDEO_COLUMNS` and
   `VIDEO_ROWS`.
2. Add `kernel/src/io/video/vga_<name>.c` defining `video_backend` (the glob
   filter matches `.*/io/video/vga_.*\.c$`).
3. Add the name to `VIDEO_TYPES` and a branch mapping it to its source and
   geometry header.

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

The software cursor keeps **one cell** of state, not a copy of the screen. The
generic layer repaints the cell the cursor vacates (writing its buffer's own
value back, so it can never change what is displayed), so the backend only needs
the cell underneath the cursor: to merge the cursor into that cell's glyph, and
to take the cell's own foreground colour the way the text-mode hardware cursor
does. It is kept current by watching `put_cells`.

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
same assertions validate both backends. Since the public API cannot read screen
content, it pins the cursor and geometry contract only.

**Pixel-exact screendump** — pins the rendered content the API cannot observe.
Boot headless with a QEMU monitor socket, `screendump`, and `cmp` against a
baseline captured before the change. Three independent boots produce
byte-identical captures, so a difference is a real change, not noise. The VGA
text baseline is 720x400; the graphical one is 640x480.

```sh
qemu-system-i386 -vga std -m 1096M -nodefaults -serial file:/dev/null \
  -drive file=build/rootfs.img,format=raw,if=ide,index=0,media=disk \
  -monitor unix:/tmp/mon.sock,server,nowait -display none \
  -kernel build/mentos/bootloader.bin &
sleep 15
printf 'screendump /tmp/shot.ppm\nquit\n' | nc -U /tmp/mon.sock
```

Note the AF_UNIX 108-byte path limit: keep the socket path short.

A useful cross-check on colour: histogram both captures. The text and graphical
backends must produce **exactly the same set of RGB values** for the same boot
log. Any divergence means a palette or attribute bug.

Plus `make qemu-test` holding at 52 tests / 0 failures with
`grep -c PANIC build/serial.log` equal to 0.
