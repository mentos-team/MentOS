# Video backend abstraction — design

Status: approved for implementation
Date: 2026-08-26
Branch: `feature/video-backend-abstraction`

## Goal

Make `kernel/src/io/video.c` backend-independent, then restore exactly one
graphical VGA backend adapted from the reference material in `.legacy-vga/`.

The existing public `video_*` API is preserved verbatim, and the VGA text
backend remains the default and must behave **identically** before and after
the refactor.

Explicitly out of scope: dynamic QEMU window resizing, framebuffer drivers,
virtio-gpu, and any other graphics feature not required by the above.

## Current state

`video.c` (885 lines) tracks the console with a raw `char *pointer` into
`0xB8000`, hardcodes `80x25`, computes every offset in units of `W2 = WIDTH * 2`
bytes, and drives the VGA CRTC cursor registers directly. Scrollback is ten
pages of raw text-mode cells.

Callers of the public API are few:

| Site | Uses |
|---|---|
| `kernel/src/klib/vsprintf.c:460` | `video_puts` (the `printf` path) |
| `kernel/src/kernel.c` | `video_init`, `video_puts`, `video_move_cursor`, `video_get_cursor_position`, `video_get_screen_size` |
| `kernel/src/io/proc_video.c` | `video_putc`, `video_puts` |

### Early-boot constraints (verified, not assumed)

1. `video_init()` runs at `kernel.c:153`, **before** `pmmngr_init`,
   `kmem_cache_init`, `paging_init` and `vmem_init`. A backend may therefore use
   **static storage only** — no allocation of any kind.
2. `printf` -> `video_puts` is reachable **before** `video_init()`, on the
   invalid-multiboot-magic path, which then panics and never reaches
   `video_init()`. Console output must not depend on any initialization having
   run.
3. `paging.c:117` identity-maps the first 1 MB ("to access video memory and
   other BIOS functions"), so both `0xB8000` and the `0xA0000` graphics aperture
   stay reachable before and after paging is enabled.
4. `kernel_panic` writes only to serial via `pr_emerg`. It reaches video
   indirectly, through the `printf`/`print_fail()` that typically precedes it.
5. There is no periodic video hook and none will be added, so the graphical
   backend must not depend on the timer. Its cursor is therefore static, not
   blinking. (The legacy `vga_update()` tick is deliberately not restored.)

## Chosen approach

Considered three options:

- **A. Backend-owned cell framebuffer.** The backend hands out a `cell*` plus
  geometry and `video.c` keeps doing `memmove`/`memset` on it. Rejected: the
  two-bytes-per-cell layout still leaks into generic code, and `video.c` has no
  way to express *which* region changed, so a graphical backend could only
  repaint the whole screen after every `putc`.
- **B. Generic cell model plus a narrow write-through backend.** Chosen.
- **C. Full VT100 terminal-emulator rewrite.** Rejected: rewriting the escape
  parser guarantees behaviour drift, contradicting the preservation requirement.

### B: generic cell model

`video.c` owns a `video_cell_t` shadow buffer as the single source of truth and
performs **all** cursor, escape-sequence, erase and scroll logic in *cell
coordinates*. Every mutation is mirrored to hardware through a small set of
ops. Backends are dumb and write-only.

Mechanically, today's `char *pointer` plus `W2` becomes an `unsigned` cell index
plus `columns`, and every `memmove`/`memset` byte count becomes a cell count
times `sizeof(video_cell_t)`. That one-to-one correspondence is what makes
byte-identical text-mode behaviour achievable.

`video.c` ends up with no knowledge of addresses, pitch, bytes per cell, I/O
ports, fonts or palettes. Scrollback works identically for both backends for
free.

## Interface

`kernel/inc/io/video_backend.h` (internal; the public `io/video.h` is unchanged):

```c
#ifndef VIDEO_GEOMETRY_HEADER
#define VIDEO_GEOMETRY_HEADER "io/video/vga_text_geometry.h"
#endif
#include VIDEO_GEOMETRY_HEADER

#if !defined(VIDEO_COLUMNS) || !defined(VIDEO_ROWS)
#error "The selected video backend must define VIDEO_COLUMNS and VIDEO_ROWS."
#endif

/// One console cell: a character and its attribute.
typedef struct {
    uint8_t character;
    uint8_t attribute;
} video_cell_t;

/// Semantic cursor styles. Backends translate these to hardware or pixels.
typedef enum {
    VIDEO_CURSOR_HIDDEN,
    VIDEO_CURSOR_BLOCK,
    VIDEO_CURSOR_UNDERLINE,
    VIDEO_CURSOR_BAR,
} video_cursor_style_t;

typedef struct {
    const char *name;
    unsigned columns;
    unsigned rows;
    int  (*init)(void);
    void (*put_cells)(unsigned column, unsigned row,
                      const video_cell_t *cells, unsigned count);
    void (*scroll)(int rows);
    void (*set_cursor_position)(unsigned column, unsigned row);
    void (*set_cursor_style)(video_cursor_style_t style);
} video_backend_t;

extern const video_backend_t video_backend;
```

Three real ops. Because `video.c` always mutates its shadow first, `put_cells`
doubles as the universal "flush this range" call, so there is no separate
`fill`, `clear` or `draw_char` op. `scroll` exists because it is the one
operation where a backend beats a cell-by-cell repaint by orders of magnitude.

### Op semantics

- `init()` returns 0 on success. Called once, from `video_init()`, **before**
  the initial `video_clear()`.
- `put_cells(column, row, cells, count)` writes `count` cells in row-major
  order starting at `(column, row)`, wrapping at `columns`. Anything past the
  last row is ignored.
- `scroll(rows)`: positive moves content **up** by `rows` rows, negative moves
  it **down**. Vacated rows are left undefined; the generic layer always
  follows with `put_cells` for them.
- `set_cursor_position(column, row)` is always called with clamped, in-range
  coordinates.
- `set_cursor_style(style)` selects the shape. `VIDEO_CURSOR_HIDDEN` is part of
  the interface but is currently unreachable through ANSI, matching today's
  behaviour (`__video_hide_cursor` exists but is never called).

### Pre-init contract

> `put_cells`, `scroll` and `set_cursor_*` may be called **before** `init()`.
> A backend whose hardware is not usable at reset must ignore those calls.

This gate lives in the backend, never in the generic layer. A generic gate
would break the text build: today's invalid-multiboot-magic `printf` writes
visibly to `0xB8000` and then panics without ever reaching `video_init()`, so
suppressing it generically would lose exactly the early diagnostic this design
must protect.

The text backend needs no gate — GRUB leaves the adapter in text mode, so early
output stays byte-identical to today. The graphical backend keeps a private
`mode_set` flag and ignores hardware ops until `init()` programs the registers.

There is **no** replay of accumulated pre-init shadow content. Today
`video_init()` calls `video_clear()`, which wipes whatever pre-init `printf`
placed on screen; `video_clear()` therefore *is* the post-init full flush, and
it flushes blanks. Pre-init shadow contents are discarded in both builds,
exactly as today.

Documented limitation: in a graphical build the invalid-multiboot-magic path
produces **serial output only**, because the mode is never set. This is
inherent and accepted.

### Colour model

`video_cell_t.attribute` is the 4-bit foreground / 4-bit background byte
(`fg = attr & 0x0F`, `bg = (attr >> 4) & 0x0F`, IBM CGA/VGA order). This is the
console's colour model, shared 1:1 by both backends, which is what keeps
`__set_color` and the ANSI lookup tables in `video.c` untouched. A backend in a
different colour space would translate inside its own `put_cells`.

### Cursor synchronisation

The generic cursor-sync helper, which replaces `video_update_cursor_position`:

1. If the cell at the cursor has `character == 0`, write `{' ', color}` there
   (quirk 3 below) and flush that cell.
2. If the cursor moved, flush the **vacated** cell from the shadow.
3. `backend.set_cursor_position(x, y)`.

Step 2 writes the shadow's own value back, so it is idempotent and cannot alter
rendered content — the text backend merely takes one redundant two-byte store
on cursor moves, keeping the screendump byte-identical. Because the generic
layer repaints the vacated cell, a software-cursor backend needs **no** cached
copy of the cell model. The graphical backend retains only `{column, row,
style}` plus the attribute of the cell last written at the cursor position
(snooped from `put_cells`), used solely to pick the cursor colour: the CRTC uses
the cell's foreground attribute today, and `UNDERLINE`/`BAR` cover only part of
the cell, so the glyph beneath must survive.

### Compile-time geometry

Each backend ships a geometry header under `kernel/inc/io/video/` defining
`VIDEO_COLUMNS` and `VIDEO_ROWS`. CMake selects it with
`-DVIDEO_GEOMETRY_HEADER="..."`; `video_backend.h` includes it indirectly and
`#error`s if the macros are absent. The **same** macros initialise the backend
struct's `.columns`/`.rows`, so reported geometry and static storage cannot
desync. No generic code ever names a backend or hardcodes a dimension.

Static storage, all in BSS:

| Buffer | Cells | Text (80x25) | Graphical (80x30) |
|---|---|---|---|
| `screen` (+1 guard cell, quirk 5) | `C*R + 1` | 4002 B | 4802 B |
| `scrollback` (`VIDEO_STORED_PAGES` = 10) | `10*C*R` | 40000 B | 48000 B |
| `original_page` | `C*R` | 4000 B | 4800 B |
| **total** | | **48002 B** | **57602 B** |

Today's equivalents total 48000 B, so the default text build grows by
**2 bytes**. The graphical build additionally carries a 4096 B `const` font, a
48 B palette and roughly 12 bytes of cursor state.

### Backend selection

Link-time, via the single `extern const video_backend_t video_backend;` symbol
defined by exactly one compiled backend. Statically initialised const data, so
it is fully valid during a pre-`video_init()` `printf`: no lazy init, no
allocation, no indirection to fail. This restores the `VIDEO_TYPE` cache
variable removed in `ea8957b`, matching the existing `SCHEDULER_TYPE` and
`KEYMAP_TYPE` idiom.

## File layout

```
kernel/inc/io/video.h                          public API, UNCHANGED
kernel/inc/io/video_backend.h                  new: internal interface
kernel/inc/io/video/vga_text_geometry.h        new: 80x25
kernel/inc/io/video/vga_graphics_geometry.h    new: 80x30   (commit 4)
kernel/src/io/video.c                          becomes generic; stays put
kernel/src/io/video/vga_text.c                 new: 0xB8000, CRTC cursor
kernel/src/io/video/vga_graphics.c             new             (commit 4)
kernel/src/io/video/vga_graphics_mode.h        new: registers  (commit 4)
kernel/src/io/video/vga_graphics_font.h        new: 8x16 font  (commit 4)
kernel/src/io/video/vga_graphics_palette.h     new: 16 colours (commit 4)
```

`video.c` deliberately stays at `kernel/src/io/video.c` rather than moving into
the new directory: it keeps `git log --follow` intact, makes the commit-2 diff
reviewable, and lets the CMake source filter target `.*/io/video/vga_.*\.c$`
without touching the generic file.

Geometry headers live under `kernel/inc/` because that is an include directory;
the private font, mode and palette headers sit beside their `.c` and are
included relatively.

## Graphical backend

Target: **640x480, 16 colours, planar, with the 8x16 font — exactly 80x30
cells.** Chosen for the simplest geometry (both divisions exact, no wasted
pixels) and because 16 colours map 1:1 onto the existing attribute byte, so the
console's colour semantics are preserved unchanged. It is a first reference
backend, not a final display mode; denser modes and fonts are a separate
evaluation.

Geometry is verified from the mode's own register values, not from comments:
`crtc.offset = 0x28` gives 40*2 = 80 bytes per row per plane = 640/8, and
`vertical_display_end = 0xDF` with `overflow = 0x3E` supplying bits 8 and 9
gives 479, i.e. 480 lines. 480/16 = 30 rows exactly. Four planes of
480*80 = 38400 bytes each fit inside the 64 KB aperture at `0xA0000`
(`gc.misc_graphics = 0x05` selects the `A0000-AFFFF` map).

Why this mode is a good fit for glyph rendering: with an 8-pixel-wide font in
planar mode, one glyph row is **exactly one byte per plane at a byte-aligned
address**. Blitting a cell is

```
for p in 0..3:
    set_plane(p)                     # one port write
    for row in 0..15:
        vram[offset + row*80] = (fg>>p & 1 ?  mask : 0)
                              | (bg>>p & 1 ? ~mask : 0)
```

four port writes and 64 byte stores per character, with no read-modify-write
and no per-pixel loop. Scrolling is four in-VRAM `memmove`s (`__set_plane` sets
both the GC read map and the sequencer write mask, so a same-plane `memmove` is
correct).

### Legacy defects to fix while adapting

Verified against register values and glyph data, not comments:

1. **Palette never scaled to the DAC's 6-bit range.** `ansi_256_palette` holds
   8-bit components written straight to `PALETTE_DATA`, so only the low 6 bits
   land: `{128, 0, 0}` maroon programs as pure **black**. Components must be
   `>> 2`.
2. **640x480 attribute-controller palette is wrong for a 16-entry DAC load.**
   `_mode_640_480_16.ac.internal_palette_registers` is the EGA-style
   `{0,1,2,3,4,5,0x14,7,0x38,...}` mapping, so attribute indices 6 and 8-15
   select DAC slots 0x14 and 0x38-0x3F, which the 16-entry load never
   initialises. Must be `{0..15}` (as the 320x200 mode already is).
3. **Glyph blitter off by one column**, and `vga_draw_string` hardcodes an 8-px
   advance regardless of font width. Both are moot in the chosen formulation:
   there is no per-pixel loop and no string helper.
4. `vga_finalize` copies 256 KB out of a buffer into which only 0x4000 bytes
   were ever saved. `finalize` is not restored — nothing calls it.
5. Palette and mode tables are non-`static` definitions in headers, relying on
   `-fcommon`. They become `static const` in the backend.
6. `__set_plane`'s `static` current-plane cache is dropped: it is stale after a
   mode set, and the port writes are negligible next to the `memmove`s.

Wrong comments confirmed in the reference material, recorded so nobody trusts
them again: `// 80x60` on the 640x480 mode is impossible (its 8x14 font gives
34 rows); `// 40x25` on 320x200 pairs with a 5x6 font giving 64x33; `// 90x60`
on 720x480 gives 90x30; and `__write_font` reads the font *out of* video memory
despite its name.

### Cursor styles

| Style | Text backend (CRTC scanlines) | Graphical backend |
|---|---|---|
| `BLOCK` | `(0, 15)` | all 16 rows filled, fg colour |
| `UNDERLINE` | `(13, 15)` | rows 13-15 |
| `BAR` | `(0, 1)` | 2-px vertical bar at the left edge |
| `HIDDEN` | cursor-start bit 5 set | nothing drawn |

`BAR` is a deliberate, documented divergence. VGA text mode cannot draw a
vertical bar, so scanlines `0..1` render as a thin horizontal sliver at the top
of the cell — the legacy `// vertical bar cursor` comment describes an intent
the hardware never delivered. The text backend keeps `(0, 1)` verbatim to
preserve behaviour; the graphical backend renders what ANSI DECSCUSR 5/6
actually means.

## Behaviour-preservation inventory

Each item is a quirk of today's `video.c` and its translation rule. This is
what "identical behaviour" means in practice.

### Fill values — three distinct ones that must not be unified

1. The blank cell is `{0x00, 0x00}`, **not** `{' ', 0x07}`. `video_clear` and
   every `ESC[J`/`ESC[K` variant `memset(..., 0, ...)`, zeroing character *and*
   attribute (black on black). `VIDEO_CELL_BLANK = {0, 0}`.
2. Erase-to-space uses the **current** colour: `__move_cursor_backward(erase=1)`
   and `DEL` write `{' ', color}` at the line end;
   `__move_cursor_forward(erase=1)` writes `{' ', color}` at the cursor.
3. `video_update_cursor_position()` **mutates the framebuffer**: if the cell at
   the cursor has `character == 0`, it writes `{' ', color}`. This is what makes
   the hardware cursor visible over an empty cell. It stays a generic cell
   mutation flushed through `put_cells`, not something a backend invents.

### Cursor model

4. `__draw_char` **inserts**: it shifts the remainder of the current line right
   by one cell and drops the last cell off the end. It does not overwrite.
5. The cursor may legally sit **one cell past the end of the screen**
   (`__move_cursor_forward` permits `pointer + 2 <= ADDR + TOTAL_SIZE`). Quirk 3
   then writes two bytes at `0xB8000 + 4000`, an out-of-bounds write landing in
   the invisible second text page, while `__get_x`/`__get_y` report `(0,0)` and
   the CRTC write clamps to `(0,24)` (the raw division gives column 0, row 25,
   and only the row is clamped). Some sequences can push the cursor further out
   still: `ESC[B` on a parked cursor sees row 0 and adds a whole row.
   Translation: the cursor is an `unsigned` index in `[0, columns*rows]`
   **inclusive**, clamped on every assignment, and the shadow carries **one
   guard cell**. Cell mutations are skipped while parked, because in the
   original they all landed outside the visible screen. Observably identical --
   reported position, rendered position and recovery path -- and the latent
   out-of-bounds writes disappear.
6. `__get_x`/`__get_y` return `0` for an out-of-range pointer in either
   direction.

### Escape sequences

7. `ESC[H`/`ESC[f` with no `;` goes home. With `;`, `atoi(...) - 1` on a zero or
   missing parameter **underflows** the `unsigned` and is then caught by the
   `>= WIDTH` clamp, so `ESC[0;0H` lands at `(79,24)`, not `(0,0)`. Preserved as
   underflow-then-clamp.
8. `ESC[A`/`ESC[B` move whole rows with asymmetric guards (`y > 0` and
   `y < rows-1`) and, unlike `__draw_char`, do **not** unscroll first.
9. `ESC[C`/`ESC[D` promote a zero or absent parameter to 1; forward never
   shifts, backward never erases.
10. Four distinct `ESC[J` behaviours: `0J` cursor to end; `1J` start to cursor
    **inclusive**; `3J` calls `video_clear()` (also wiping scrollback);
    `2J`/default clears the screen, resets the cursor home and `scrolled_lines`
    but **preserves** scrollback. Only `2J` moves the cursor.
11. Three `ESC[K` behaviours (`0` cursor to line end, `1` line start to cursor
    inclusive, `2` whole line), none of which touch the cursor or trigger a
    cursor update.
12. `ESC[m`: an empty body calls `__set_color(0)`; otherwise `strtok_r` on `;`.
    `__set_color`'s table is already backend-independent and moves verbatim.
13. `ESC[s`/`ESC[u`: restore only if the saved value lies in `[start, end)` —
    an **exclusive** upper bound, unlike quirk 5's inclusive cursor. The saved
    value starts at home.
14. `ESC[S` calls `video_scroll_down` and `ESC[T` calls `video_scroll_up` —
    **inverted** relative to the letters. Both `return` early, skipping the
    trailing cursor update, and clamp a negative parameter to 0, so a bare
    `ESC[S` scrolls zero lines (unlike `C`/`D`).
15. `ESC c` (RIS): `video_clear()`, `color = 0x07`, cursor style `BLOCK`.
16. Parser state machine: `\033` sets `escape_index = 0` and zeroes the buffer;
    at index 0, `c` triggers RIS, `[` advances to index 1, anything else aborts;
    the overflow guard is `sizeof(escape_buffer) - 1`; dispatch happens on
    `isalpha`; the command character is stripped via
    `escape_buffer[--escape_index] = 0`; parameters are read from
    `&escape_buffer[1]`.

### Character dispatch and batching

17. `video_putc`: `\n` -> `video_new_line`; `\b` ->
    `__move_cursor_backward(true, 1)`; `\r` -> `video_cartridge_return`; `127`
    -> in-line delete-shift then `{' ', color}` at the line end, with its own
    conditional cursor update and an early `return`; printable `0x20..0x7E` ->
    `__draw_char`; everything else is silently ignored **with no cursor
    update**.
18. `batch_cursor_updates`, set by `video_puts`, suppresses per-character cursor
    updates and therefore also suppresses quirk 3's space materialisation for
    intermediate characters. It changes what lands in the framebuffer, so it
    must be preserved rather than optimised away.

### Scrollback

19. `__shift_screen_up` feeds `upper_buffer` only when `scrolled_lines == 0`,
    copying screen row 0 into the **last** row of the buffer.
    `__shift_buffer(buf, lines, dir)` moves `lines - 1` rows.
    `video_shift_one_line_up` has three cases (past-end, scrolled, no-op).
    `video_shift_one_line_down` snapshots the live screen into `original_page`
    on the **first** scroll only and stops at `STORED_PAGES * rows`.
    `__draw_char`, `video_new_line` and `video_cartridge_return` each unscroll
    first.
20. `video_init`'s colour-table build routes code 0 and `30-37`/`90-97` to
    `fg_color_map` and everything else to `bg_color_map`, so
    `fg_color_map[0] = 7`. Then `video_clear()`, then what becomes
    `set_cursor_style(BLOCK)`.

### Quirks deliberately left unfixed

Quirks **5**, **7** and **14** are pre-existing defects. The mandate is
identical behaviour, so they are preserved and recorded here for a separate
change rather than fixed. Quirk 5's *memory-safety* aspect is neutralised by
the guard cell without altering observable behaviour.

## Verification

Two complementary gates, because neither alone is sufficient.

1. **Characterization suite** (`kernel/src/tests/unit/test_video.c`, registered
   in `kernel/src/tests/runner.c`). Asserts through the **public API only**, so
   it is backend-independent by construction and runs unchanged against both
   backends. The public API cannot read screen content, so this suite pins
   **cursor position and geometry**: quirks 4-11, 13, 15 and 17.
2. **Pixel-exact screendump.** Boot to the `Username:` prompt, capture via the
   QEMU monitor `screendump`, and `cmp` against a 720x400 PPM baseline taken
   before any edit. This pins the **rendered content** quirks (1, 2, 3, 12, 18,
   19) that the API cannot observe.

Plus: a clean build under the existing `-Werror -Wpedantic -pedantic-errors
-Wshadow`, and `make qemu-test` holding at **52 tests / 0 failures** with
`grep -c PANIC build/serial.log` equal to 0.

Baseline captured before any edit: 52 tests, 0 failures, 0 panics, and
`base.ppm` at 720x400.

**Hard stop:** if the default VGA text build is not pixel-identical after the
abstraction refactor, stop and report rather than continue.

Graphical backend acceptance: rebuild with
`-DVIDEO_TYPE=VGA_MODE_640_480_16`, boot, screendump, and confirm a 640x480
capture with legible 80x30 text and correct 16 colours.

## Commit sequence

1. `test(video): characterize existing console behaviour` — the suite lands
   **first** and must pass against the current, unmodified implementation, then
   stay unchanged through the refactor.
2. `refactor(video): make video.c backend-independent` — add
   `video_backend.h`, the geometry mechanism and `vga_text.c`; convert
   `video.c` to the cell model. `video_backend.h` defaults
   `VIDEO_GEOMETRY_HEADER` to the text backend, so no CMake change is needed
   yet. Gated on both verification gates.
3. `build(video): add VIDEO_TYPE backend selection` — restore the `ea8957b`
   cache-variable pattern plus source filtering and the
   `VIDEO_GEOMETRY_HEADER` define, with `VGA_TEXT_MODE` the only choice.
   Exercises the selection mechanism with zero behaviour change.
4. `feat(video): add 640x480x16 VGA backend` — mode registers, palette, 8x16
   font, planar glyph blitter, per-plane scroll, software cursor, pre-init
   self-gating. Adds `VGA_MODE_640_480_16` to `VIDEO_TYPES`.
5. `docs(video): document video backend abstraction` — add
   `docs/maintainer/video-backends.md` plus the `AGENTS.md` map entry, per
   repo convention.
