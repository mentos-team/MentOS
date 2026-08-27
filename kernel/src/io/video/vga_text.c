/// @file vga_text.c
/// @brief VGA text-mode video backend.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// This is the default backend and the only one whose hardware is usable
/// straight out of reset: the bootloader leaves the adapter in an 80x25 text
/// mode, so writes to the framebuffer are visible before init() has run. That
/// is what keeps the early printf() on the invalid-multiboot-magic path
/// visible, and it is why this backend needs no pre-initialization gate.
///
/// Everything specific to VGA text mode lives here: the framebuffer address,
/// the two-bytes-per-cell layout and the CRTC cursor registers.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[VGATXT]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "io/port_io.h"
#include "io/video_backend.h"
#include "stddef.h"
#include "string.h"

/// Base address of the VGA text-mode framebuffer.
#define VGA_TEXT_ADDRESS         0xB8000U

/// VGA CRTC index register port.
#define VGA_CRTC_INDEX           0x3D4
/// VGA CRTC data register port.
#define VGA_CRTC_DATA            0x3D5
/// VGA cursor start register index.
#define VGA_CURSOR_START         0x0A
/// VGA cursor end register index.
#define VGA_CURSOR_END           0x0B
/// VGA cursor location low register index.
#define VGA_CURSOR_LOCATION_LOW  0x0F
/// VGA cursor location high register index.
#define VGA_CURSOR_LOCATION_HIGH 0x0E
/// Bit of the cursor start register that switches the cursor off.
#define VGA_CURSOR_DISABLE       0x20

/// Total number of cells on screen.
#define VGA_TEXT_CELLS           (VIDEO_COLUMNS * VIDEO_ROWS)

/// @brief The framebuffer, addressed as an array of console cells.
///
/// A VGA text-mode cell is a character byte followed by an attribute byte,
/// which is exactly the layout of video_cell_t, so the framebuffer can be
/// addressed directly with no conversion.
static video_cell_t *const vga_text_memory = (video_cell_t *)VGA_TEXT_ADDRESS;

/// @brief Compile-time check that a console cell maps 1:1 onto a VGA cell.
///
/// Fails the build with a negative array size if the assumption above breaks.
typedef char vga_text_cell_layout_check[(sizeof(video_cell_t) == 2) ? 1 : -1];

/// @brief Writes one CRTC register.
/// @param index The register index.
/// @param value The value to write.
static inline void __vga_text_write_crtc(uint8_t index, uint8_t value)
{
    outportb(VGA_CRTC_INDEX, index);
    outportb(VGA_CRTC_DATA, value);
}

/// @brief Sets the cursor scan lines and makes sure it is enabled.
/// @param first The first scan line of the cursor (0-15).
/// @param last The last scan line of the cursor (0-15).
static void __vga_text_set_cursor_scanlines(uint8_t first, uint8_t last)
{
    // An inverted range would hide the cursor; fall back to a full block.
    if (first > last) {
        first = 0;
        last  = 15;
    }
    // Writing the start register with bit 5 clear also enables the cursor.
    __vga_text_write_crtc(VGA_CURSOR_START, first & 0x1FU);
    __vga_text_write_crtc(VGA_CURSOR_END, last & 0x1FU);
}

/// @brief Hides the hardware cursor.
static void __vga_text_hide_cursor(void)
{
    outportb(VGA_CRTC_INDEX, VGA_CURSOR_START);
    uint8_t cursor_start = inportb(VGA_CRTC_DATA);
    __vga_text_write_crtc(VGA_CURSOR_START, cursor_start | VGA_CURSOR_DISABLE);
}

/// @brief Brings up the backend.
/// @return Always 0: the adapter is already in text mode when we get here.
static int vga_text_init(void) { return 0; }

/// @brief Writes cells into the framebuffer.
/// @param column Column of the first cell.
/// @param row Row of the first cell.
/// @param cells The cells to write.
/// @param count How many cells to write.
static void vga_text_put_cells(unsigned column, unsigned row, const video_cell_t *cells, unsigned count)
{
    if ((cells == NULL) || (column >= VIDEO_COLUMNS) || (row >= VIDEO_ROWS)) {
        return;
    }
    unsigned first = (row * VIDEO_COLUMNS) + column;
    // Clip at the end of the screen rather than running past the framebuffer.
    if (count > (VGA_TEXT_CELLS - first)) {
        count = VGA_TEXT_CELLS - first;
    }
    if (count == 0) {
        return;
    }
    memcpy(&vga_text_memory[first], cells, count * sizeof(video_cell_t));
}

/// @brief Moves the displayed content vertically.
/// @param rows Positive moves content up, negative moves it down.
static void vga_text_scroll(int rows)
{
    if (rows == 0) {
        return;
    }
    unsigned distance = (rows > 0) ? (unsigned)rows : (unsigned)(-rows);
    // Nothing would survive the move; the caller repaints what it needs.
    if (distance >= VIDEO_ROWS) {
        return;
    }
    unsigned moved_cells = (VIDEO_ROWS - distance) * VIDEO_COLUMNS;
    unsigned offset      = distance * VIDEO_COLUMNS;
    if (rows > 0) {
        memmove(vga_text_memory, &vga_text_memory[offset], moved_cells * sizeof(video_cell_t));
    } else {
        memmove(&vga_text_memory[offset], vga_text_memory, moved_cells * sizeof(video_cell_t));
    }
}

/// @brief Places the hardware cursor.
/// @param column The column.
/// @param row The row.
static void vga_text_set_cursor_position(unsigned column, unsigned row, video_cell_t cell)
{
    // The hardware cursor is drawn by the CRTC over whatever the cell already
    // holds, so this backend never touches the cell and never has an overlay to
    // take away again.
    (void)cell;

    if (column >= VIDEO_COLUMNS) {
        column = VIDEO_COLUMNS - 1;
    }
    if (row >= VIDEO_ROWS) {
        row = VIDEO_ROWS - 1;
    }
    uint32_t position = (row * VIDEO_COLUMNS) + column;
    __vga_text_write_crtc(VGA_CURSOR_LOCATION_LOW, (uint8_t)(position & 0xFFU));
    __vga_text_write_crtc(VGA_CURSOR_LOCATION_HIGH, (uint8_t)((position >> 8U) & 0xFFU));
}

/// @brief Selects the cursor appearance.
/// @param style The style to adopt.
///
/// The scan-line ranges below are the ones this console has always used. Note
/// that VIDEO_CURSOR_BAR maps to scan lines 0-1, which the hardware draws as a
/// thin horizontal sliver across the top of the cell: VGA text mode cannot draw
/// a vertical bar at all. That approximation is preserved deliberately, so text
/// mode keeps rendering exactly as before.
///
/// The blink request is deliberately ignored. The CRTC cursor blinks in hardware
/// and offers no way to stop it, so honouring a steady request is not possible
/// and pretending otherwise would change long-standing behaviour.
static void vga_text_set_cursor_style(video_cursor_style_t style)
{
    switch (style.shape) {
    case VIDEO_CURSOR_HIDDEN:
        __vga_text_hide_cursor();
        break;
    case VIDEO_CURSOR_UNDERLINE:
        __vga_text_set_cursor_scanlines(13, 15);
        break;
    case VIDEO_CURSOR_BAR:
        __vga_text_set_cursor_scanlines(0, 1);
        break;
    case VIDEO_CURSOR_BLOCK:
    default:
        __vga_text_set_cursor_scanlines(0, 15);
        break;
    }
}

/// @brief The VGA text-mode backend.
const video_backend_t video_backend = {
    .name                = "vga-text",
    .columns             = VIDEO_COLUMNS,
    .rows                = VIDEO_ROWS,
    .init                = vga_text_init,
    .put_cells           = vga_text_put_cells,
    .scroll              = vga_text_scroll,
    .set_cursor_position = vga_text_set_cursor_position,
    .set_cursor_style    = vga_text_set_cursor_style,
    // No blink op: the CRTC cursor blinks in hardware, so there is nothing for
    // the console to drive.
    .cursor_blink        = NULL,
};
