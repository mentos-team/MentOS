/// @file video_backend.h
/// @brief Internal interface between the generic console and a video backend.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// The generic console layer (`video.c`) owns all terminal state: the cell
/// contents, the cursor, the escape-sequence parser and the scrollback. A
/// backend knows only how to materialize that state on hardware. It is
/// deliberately write-only and stateless apart from what it needs to draw.
///
/// This header is internal. Callers outside the video layer use `io/video.h`.

#pragma once

#include "io/video.h"
#include "stdbool.h"
#include "stdint.h"

/// @brief Path of the geometry header belonging to the selected backend.
///
/// Every backend ships a header defining VIDEO_COLUMNS and VIDEO_ROWS. The
/// build system points this macro at the one being compiled; the generic layer
/// sizes its static state from those macros and never assumes a value. The
/// default keeps the VGA text backend usable without any build-system support.
#ifndef VIDEO_GEOMETRY_HEADER
#define VIDEO_GEOMETRY_HEADER "io/video/vga_text_geometry.h"
#endif

#include VIDEO_GEOMETRY_HEADER

#if !defined(VIDEO_COLUMNS) || !defined(VIDEO_ROWS)
#error "The selected video backend must define VIDEO_COLUMNS and VIDEO_ROWS."
#endif

/// @brief One console cell.
///
/// The attribute is a 4-bit foreground index in the low nibble and a 4-bit
/// background index in the high nibble, in IBM CGA/VGA order. This is the
/// console's colour model, shared by every backend, which is what lets the
/// generic ANSI colour tables stay backend-independent. A backend working in a
/// different colour space translates inside its own put_cells().
typedef struct {
    uint8_t character; ///< The character code.
    uint8_t attribute; ///< Foreground index | (background index << 4).
} video_cell_t;

/// @brief Cursor appearance, expressed semantically.
///
/// Backends translate these to whatever they have: CRTC scanline registers for
/// a hardware cursor, or drawn pixels for a software one. The generic layer
/// deliberately knows nothing about scanlines.
typedef enum {
    VIDEO_CURSOR_HIDDEN,    ///< Not drawn at all.
    VIDEO_CURSOR_BLOCK,     ///< Fills the whole cell.
    VIDEO_CURSOR_UNDERLINE, ///< A line along the bottom of the cell.
    VIDEO_CURSOR_BAR,       ///< A thin bar; see the note in the backends.
} video_cursor_shape_t;

/// @brief A cursor style: a shape, and whether it blinks.
///
/// Blink is carried separately rather than folded into the shape because the
/// two backends need it differently. A hardware text cursor blinks on its own
/// and cannot be told not to, so the VGA text backend ignores this and keeps
/// behaving exactly as it always has. A software cursor is drawn pixels and
/// blinks only if something toggles it, so a graphical backend does need to
/// know which was asked for.
typedef struct {
    video_cursor_shape_t shape; ///< The shape to draw.
    bool_t blinking;            ///< Whether it should blink.
} video_cursor_style_t;

/// @brief Operations a video backend must provide.
///
/// Exactly one backend is compiled into the kernel and defines the
/// `video_backend` symbol below. Because that definition is statically
/// initialized const data, it is already valid during a printf() issued before
/// video_init() has run: no lazy initialization, no allocation, nothing that
/// can fail at the point where early diagnostics are printed.
typedef struct {
    /// Human-readable backend name, for diagnostics.
    const char *name;

    /// Console width in cells. Must equal VIDEO_COLUMNS.
    unsigned columns;

    /// Console height in cells. Must equal VIDEO_ROWS.
    unsigned rows;

    /// @brief Brings the hardware into a usable state.
    /// @return 0 on success, a negative value on failure.
    ///
    /// Called once, from video_init(), before the console pushes anything. The
    /// generic layer clears the screen immediately afterwards, so a backend
    /// need not display anything itself.
    int (*init)(void);

    /// @brief Finishes initialization once memory management exists.
    /// @return 0 on success, a negative value on failure.
    ///
    /// Optional: NULL when a backend has nothing to defer, which is the case for
    /// every backend whose hardware is reachable through the first megabyte.
    ///
    /// It exists for hardware that cannot be touched at video_init() time at
    /// all. A linear framebuffer lives high above RAM, in a PCI BAR that the
    /// bootloader's page directory does not map, and video_init() runs before
    /// pmmngr_init(), kmem_cache_init() and paging_init(), so there is nothing
    /// there to map it with -- and paging_init() then switches to a page
    /// directory of its own, which would drop a hand-built mapping anyway.
    ///
    /// Called once from video_late_init(), after paging_init() has succeeded, so
    /// mem_upd_vm_area() and the page-table cache are both available. A backend
    /// that defers must stay completely inert until this has returned 0; see the
    /// pre-initialization contract on `video_backend` below.
    ///
    /// The generic layer repaints its whole cell buffer afterwards, so nothing
    /// printed in between is lost: the buffer is the source of truth and it has
    /// been recording all along.
    int (*late_init)(void);

    /// @brief Writes cells to the display.
    /// @param column Column of the first cell.
    /// @param row Row of the first cell.
    /// @param cells The cells to write.
    /// @param count How many cells to write.
    ///
    /// Cells are written in row-major order starting at (column, row), wrapping
    /// to the next row at `columns`. Anything past the last row is ignored.
    ///
    /// The generic layer always updates its own copy first, so this doubles as
    /// the universal "flush this range" call: there is deliberately no separate
    /// fill, clear or draw-character operation.
    void (*put_cells)(unsigned column, unsigned row, const video_cell_t *cells, unsigned count);

    /// @brief Moves displayed content vertically, preserving it.
    /// @param rows Positive moves content up, negative moves it down.
    ///
    /// This exists because scrolling is the one operation where a backend beats
    /// a cell-by-cell repaint by orders of magnitude. The rows uncovered at the
    /// far edge are left undefined; the generic layer always follows up with
    /// put_cells() for them.
    void (*scroll)(int rows);

    /// @brief Optional: begins a run of updates that may be shown all at once.
    /// @brief Optional: ends it, and shows what the run changed.
    ///
    /// Both may be NULL, and are for a backend where making a change *visible*
    /// costs a device round trip -- put_cells() into video memory is already as
    /// cheap as it gets, and such a backend wants neither hook. Between the two
    /// calls the backend may draw as it likes but need not present anything; at
    /// end_batch() the display must show every change the run made, and be
    /// identical to what a run of unbatched calls would have produced.
    ///
    /// The generic layer brackets each run of output it knows to be one unit --
    /// one write() to the console, one video_puts() -- and counts nesting, so a
    /// backend sees one outermost pair. Nothing else is promised: a backend that
    /// implements them must still present eventually if they are never called,
    /// which is what keeps single-character output prompt.
    void (*begin_batch)(void);
    void (*end_batch)(void);

    /// @brief Places the cursor.
    /// @param column The column, always already in range.
    /// @param row The row, always already in range.
    /// @param cell The cell the cursor now sits on.
    ///
    /// The cell is passed because a backend that draws its own cursor has to be
    /// able to take it away again, and restoring what was underneath is the only
    /// way to do that. It cannot recover the cell from the display -- a drawn
    /// glyph is not reversible -- and keeping a copy of the whole screen to look
    /// it up in would duplicate state the generic layer already owns. One cell
    /// is all that is needed, so one cell is what it is given.
    void (*set_cursor_position)(unsigned column, unsigned row, video_cell_t cell);

    /// @brief Selects the cursor appearance.
    /// @param style The style to adopt.
    void (*set_cursor_style)(video_cursor_style_t style);

    /// @brief Prepares to materialize a different cell geometry.
    /// @param columns The new console width in cells.
    /// @param rows The new console height in cells.
    /// @return 0 when the backend is ready for that geometry, negative otherwise.
    ///
    /// Optional: NULL means the backend cannot change shape, and the generic
    /// layer then refuses every resize. Both fixed backends leave it NULL and
    /// are unaffected by any of this.
    ///
    /// The currency is **cells**, not pixels. A backend owns its font and works
    /// out the pixel geometry from these; nothing generic knows a font size.
    /// That is also what lets a future font change be the same operation as a
    /// display change: both arrive here as a different pair of cell counts.
    ///
    /// Called from process context, and required to be **transactional**: it may
    /// allocate and talk to its device, but it must not disturb what is
    /// currently displayed. On success the backend is expected to keep showing
    /// the old geometry until the generic layer repaints, and to switch on the
    /// first put_cells() afterwards -- so a failure here, or a failure to
    /// allocate the new console, leaves a working display either way.
    int (*set_geometry)(unsigned columns, unsigned rows);

    /// @brief Adopts a different font size and reshapes the console to match.
    /// @param reset Start from the default size rather than the current one.
    /// @param steps Signed number of size steps to move; positive is larger.
    /// @return 0 when the font changed or was already right, negative otherwise.
    ///
    /// Optional: NULL means the backend cannot change font, and the generic layer
    /// then drops such requests where they are made rather than carrying them.
    /// The VGA text adapter cannot (its font is the hardware's, and changing it
    /// would mean reprogramming the font RAM and the CRTC), and neither fixed
    /// graphical backend can, because its mode is fixed and its geometry with it.
    ///
    /// Called from video_service_pending(), so process context, and expected to
    /// own the whole transition: choose the font, work out the cell counts its
    /// own scanout now divides into, and call video_change_geometry(). The font
    /// must only become the one being drawn with if that succeeds -- a refused or
    /// unaffordable geometry has to leave the display exactly as it was.
    ///
    /// A step count rather than a direction so that requests cannot be lost: two
    /// "larger" that arrive before a service get here as one call for two steps.
    int (*request_font)(bool_t reset, int steps);

    /// @brief Gives the backend process context for work it cannot do elsewhere.
    ///
    /// Optional: NULL when a backend has nothing to defer.
    ///
    /// Called from video_service_pending(), before a pending resize is applied,
    /// so a backend that learns of a display change in an interrupt handler can
    /// do the part that needs to allocate or talk to its device here and then
    /// call video_request_resize() -- which this same call will then act on.
    ///
    /// Process context only, and must therefore never be reached from an
    /// interrupt handler.
    void (*service)(void);

    /// @brief Advances a software cursor's blink, once per timer tick.
    ///
    /// Optional: NULL when a backend has nothing to do, which is the case for
    /// any backend whose cursor blinks in hardware. Called from the timer
    /// interrupt via video_cursor_blink_tick(), so it must be cheap and must not
    /// sleep, allocate or take locks.
    ///
    /// The rate is the backend's business, not the console's; this is simply the
    /// tick it counts.
    void (*cursor_blink)(void);
} video_backend_t;

/// @brief The backend compiled into this kernel.
///
/// Defined by exactly one backend source file, selected at build time. The
/// generic layer never names a concrete backend.
///
/// Pre-initialization contract: put_cells(), scroll() and the cursor operations
/// may be called BEFORE init(), and -- for a backend that defers -- before
/// late_init() too. A backend whose hardware is not usable yet must ignore those
/// calls, touching no device memory and no device registers. The generic layer
/// records every mutation in its own copy regardless, so nothing is lost from
/// its point of view, and the repaint at the end of video_late_init() puts all
/// of it on the display at once.
///
/// This gate belongs to the backend, never to the generic layer: the VGA text
/// adapter is usable straight out of reset, and suppressing writes generically
/// would silence the early printf() on the invalid-multiboot-magic path, which
/// panics without ever reaching video_init().
extern const video_backend_t video_backend;

/// @brief Hands the console over to a different backend.
/// @param next The backend to promote.
/// @return 0 on success, a negative value on failure.
///
/// Exists for a display that becomes available only after the one the machine
/// booted on: the boot backend keeps working while the better one is brought up,
/// and only takes a back seat once that has succeeded.
///
/// The promotion is transactional. `next->late_init()` runs first and the active
/// backend is only changed once it has returned successfully, so a failure
/// anywhere in bringing up the new hardware leaves the previous backend still
/// displaying a working console. On success the whole console is repainted
/// through the new backend, so it shows the content the user was already
/// looking at rather than coming up blank.
///
/// `next` must report the console's current geometry; a mismatch is refused.
///
/// This is internal to the video layer. It is declared here rather than in
/// io/video.h because only a backend has any reason to call it, and only this
/// header defines the type it takes.
int video_promote_backend(const video_backend_t *next);

/// @brief Reshapes the console, now, from a backend.
/// @param columns The new width in cells.
/// @param rows The new height in cells.
/// @return 0 on success, a negative value on failure.
///
/// The same transactional resize video_service_pending() performs, exposed for a
/// backend that has to drive one itself. That is what a font change is: the
/// backend is the only thing that knows the new cell size, so it works out the
/// geometry and asks for it, instead of the generic layer asking for a geometry
/// the backend then reinterprets.
///
/// Unlike a resize request, this does not skip a geometry equal to the current
/// one: a font change has to repaint even when the cell counts happen to land in
/// the same place, because the cells are a different size. On failure nothing has
/// changed and the console is still displaying.
///
/// **Process context only**, and never from inside put_cells() or any other
/// backend operation the console calls: it allocates and it repaints.
int video_change_geometry(unsigned columns, unsigned rows);
